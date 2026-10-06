# ANALYSIS.md

## 1. Three categories and what their languages pay

### 1a. Arrays: Python `list`

Our `dt_array` stores its `dt_value` elements contiguously and has a fixed length after construction. Indexing checks the declared bounds and converts the index to an offset by subtracting the lower bound, so an array can use indices such as `-1..1` or `1..3`. Once the offset is known, reading or writing an element is constant time. The `dt_array` interface has no resize operation, so changing the number of elements requires creating a new array and copying the elements to keep.

A Python `list` is a resizable array of references to Python objects. It can grow with operations such as `append`, usually in amortized constant time, by reserving extra capacity. This spare capacity and the object references require additional memory. For integer elements, each slot refers to a separate Python `int` object, adding per-element object overhead and an additional level of indirection. Our array stores each `dt_value` directly in its element block, so it avoids allocating a separate object for each integer. However, each `dt_value` still requires space for its tag and union storage.

Both provide constant-time indexed access in the usual case, but Python list access also involves interpreter and reference-handling overhead. Python's flexibility is useful when the collection size changes frequently, while its per-object memory cost can become more noticeable for large collections of small values. Our array's fixed size is a limitation when elements need to be appended or removed, but its lower-bound support is useful when indices have meaningful values and do not naturally start at zero.

### 1b. Integers: Python `int`

Our `dt_int` uses C's `long long`: a fixed 64-bit (8-byte) value stored directly inside the `dt_value`, with no heap allocation. It can represent numbers up to about ±2⁶³ (LLONG_MAX = 2⁶³ − 1). Any result outside that range returns `DT_ERR_OVERFLOW`, and we check this before the arithmetic runs. Addition costs one comparison and one CPU add.

Python's `int` never overflows. Every int is a heap object: a header (reference count, type pointer, size) followed by the value stored in 30-bit chunks. When a number outgrows its chunks, Python allocates more. Even a small int like `5` costs about 28 bytes, plus an 8-byte pointer wherever it is stored. That is about 4.5× the memory of ours. Adding is also slower. Python must follow both pointers, check each number's size, loop over the chunks with a carry, and allocate a new object for the result, because Python ints are immutable.

So Python pays in memory and speed to never report overflow. We give up numbers beyond ±2⁶³ to stay small and fast. You would notice Python's cost in large collections, like a list of a million ints (about 36 MB versus 8 MB in a C array), and in tight arithmetic loops, where every operation allocates. You would notice our cost the moment a real value, like a factorial or a cryptographic key, needs a 65th bit.

### 1c. Associative arrays: Python `dict`

Our `dt_map` uses 64 fixed buckets with chaining. Each bucket is a linked list of entries, and a separate array of pointers keeps insertion order. The table never resizes. With a few keys, lookup is close to O(1): hash the key, pick a bucket, compare one or two keys. As the map grows, the chains grow with it. At 10,000 keys, each chain holds about 156 entries on average, so one lookup costs about 156 `strcmp` calls. That is effectively O(n), not O(1). We also recompute the FNV-1a hash of the key on every put, get and remove.

Python's `dict` avoids both problems. It uses open addressing: on a collision it probes another slot instead of chaining. It stores each entry's hash next to its key and value, and it resizes when the table is about 2/3 full, which keeps lookups near O(1) at any size. The stored hash means a lookup can compare hashes before comparing full keys, and a resize never recomputes a hash.

Python pays for this in memory and in occasional pauses. At least a third of the table is always empty slots. Every entry carries an extra 8-byte hash, and every key and value is a pointer to a separate heap object. When the table fills, a resize copies every entry into a new table at once, so one unlucky insert is much slower than the rest. You'd notice Python's cost in memory use for millions of small entries, and as rare latency spikes during inserts. You'd notice ours as lookups that slow down steadily once the map holds more than a few hundred keys. For short keys like `"beta"`, recomputing the hash is cheap, so storing it only pays off for long keys or frequent resizes.

## 2. The hand-written tag check

In `dt_value_as_int` we check `v.tag != DT_INT` before we read `v.as.integer`. C does not force that check. Nothing stops code from reading `v.as.string` while the tag says `DT_INT`, and the compiler accepts it. A missed check shows up only at run time, and only if a case file reaches that line. Even then, the symptom may be a crash or plausible garbage instead of a clear error. In Rust, the payload of an `enum` can only be reached through `match` or `if let`. A read that skips the check does not compile. If we add an 11th variant, every `match` that does not handle it fails to compile (error E0004). In C, adding an 11th `dt_tag` compiles everywhere. At best, `-Wswitch` warns about a `switch` with no `default`.

The C version lets us do three things a checking compiler would not:

1. **Skip a check we "know" is redundant.** Once code has switched on the tag, it can read the member directly with no second check. This saves almost nothing, because Rust's `match` checks once and binds the payload in the same step.
2. **Read the same bytes as another type (type punning).** The union lets us view `as.integer` and `as.string` as the same 8 bytes. Language runtimes use this on purpose. Some store the tag inside unused bits of a pointer or a NaN, which makes every value smaller and faster to pass around.
3. **Leave cases unhandled.** A tag set can grow without editing every function that reads it. This makes prototyping faster, but each skipped reader becomes a hidden bug.

Our position: only the second one is worth wanting, and only in low-level code like an interpreter's value representation, where layout control matters. Even there, Rust still allows it inside an `unsafe` block, so the real difference is the default. Rust makes the safe read the default and marks the unsafe one. C makes them look identical. For a library like ours, the compile-time check is better. Every `DT_ERR_TAG` we return at run time is a mistake Rust would have caught before the program ran.

## 3. Dropping the insertion-order array

Our `dt_map` keeps an `order` array, with one pointer per key, beside the 64 bucket chains. Lookups never read it. Only `dt_map_key_at`, and through it the printer, does.

**The design without it:** delete `order` and `order_cap`. Make `dt_map_key_at(m, i)` walk the buckets from 0 to 63 and down each chain, counting entries until it reaches the i-th one.

**What it saves:**

- Memory: one 8-byte pointer per key, plus the spare capacity from doubling (up to 2× the key count), plus the `realloc` calls as the array grows.
- Time on remove: today `dt_map_remove` searches `order` for the entry and `memmove`s every later pointer one slot left. That is O(n) on every remove. Without the array, remove only unlinks from one chain.

**What breaks:**

- Print order becomes bucket order. That order comes from FNV-1a mod 64, and within a chain, newest first, since we insert at the head. `{"alpha" -> 1, "beta" -> 2}` could print with `beta` first. FNV-1a has no random seed, so the order is the same on every run. But it is meaningless to a reader, and it changes if we ever change the bucket count or the hash.
- Nobody can write an `.expected` file without running the code first, so `map_basics` (re-put keeps position, delete and re-insert moves a key to the end) cannot be expressed at all.
- `dt_map_key_at` becomes O(64 + n) per call. The printer calls it once per key, so printing a map becomes O(n²) instead of O(n).

**Would we ship it?** Not for this library. Its output has to be stable and testable, and users read printed maps. We would ship it for a lookup-only map that is never iterated, such as an internal cache or symbol table. There the order array is pure overhead. Go goes further: it deliberately randomizes map iteration order so that no code can depend on it. Python went the other way. It made insertion order a language guarantee in 3.7, because programs had already started depending on it. Python also showed that order can be almost free. Its dict keeps entries in one dense array in insertion order, and the hash table only stores indexes into that array. That design would give us order without a second structure. Remove would mark a slot as deleted instead of shifting every later pointer.

## 4. Access after release vs. an unreleased allocation

Access after release occurs when an attempt is made to access memory after it has been freed. An unreleased allocation is an omission: no single operation necessarily causes an error, but the allocation is still owned when the program checks its final state.

A real use-after-free can threaten correctness and security, often unpredictably. An unreleased allocation primarily threatens capacity, since repeated leaks can increase memory use over time. How much each matters depends on the setting: in a long-running server both can be serious, while in a short-lived command-line tool a leak has less time to accumulate. A large leak can still raise peak memory use before the tool exits, and a use-after-free remains dangerous if execution reaches the faulty access.

**`dt_ref` manages these cases.** In `dt_ref.c`, a reference is represented by:

```c
struct dt_ref {
    dt_value *cell;
    bool      released;
};
```

The reference owns the heap-allocated `cell`, and the `released` flag records whether that cell has been freed. The `struct dt_ref` object is allocated separately from the cell, and `p` is a pointer to that object. `dt_ref_new` allocates both the `struct dt_ref` object and the cell. `dt_ref_release` frees the cell, while `dt_ref_destroy` eventually frees the `struct dt_ref` object.

When `dt_ref_release` is called on a live reference, it does this:

```c
free(p->cell);
p->cell = NULL;
p->released = true;
```

A later `dt_ref_borrow` checks the flag before reading the cell:

```c
if (p->released) {
    return DT_ERR_RELEASED;
}
*out = *p->cell;
```

Because the flag is checked first, the dereference is never reached after release. This implementation rejects the borrow instead of reading freed memory. `dt_ref_release` has a similar guard against a second release, preventing a double free of the cell. Setting `p->cell` to `NULL` clears the freed cell's address, while checking `p->released` prevents `dt_ref_borrow` from reading that cell. These protections apply when the reference operations are used with a live `struct dt_ref` object; they do not make arbitrary stale-pointer use safe.

An unreleased reference is handled differently. The driver checks for unreleased references in its bindings after command execution finishes successfully. It reports `DT_ERR_LEAK` if any are found. This is not a general audit of every allocation, and the driver skips the check if a command has already failed. Cleanup still runs: `dt_ref_destroy` releases any cell still held by a reference, then frees the `struct dt_ref` object. Thus, `DT_ERR_LEAK` means the reference was still unreleased at the check; it does not mean the memory remains allocated after the driver finishes cleanup.

**Damage in a long-running server:**

In unguarded code, access after release is a threat to correctness and security. After a block is freed, the allocator may reuse it for a later allocation. A stale read may then observe data written by the new owner, which in a server could belong to another user. A stale write may corrupt an unrelated object, and a double free can damage the allocator's bookkeeping. These outcomes are unpredictable, and symptoms can appear long after the faulty operation. Allocation activity in a busy server can increase the chance that freed memory is reused before a stale access. Restarting may clear immediate effects, but it does not fix the underlying bug.

An unreleased allocation does not itself invalidate a live object. Its primary risk is growing resource use: repeated leaks accumulate for as long as the allocations remain unreleased, raising memory consumption and potentially slowing the application or causing allocation failures. The impact depends on how often the leak occurs, how much memory each allocation uses, and how long the server runs. If a client can trigger the leaking path, the leak may become a denial-of-service risk. Restarting frees accumulated memory, but it does not fix the leak, so the problem can return.

Neither failure is always worse. A persistent leak can take a service down, while a use-after-free in rarely executed code may never occur. The difference is in the kind of risk: a leak threatens capacity and may appear as steadily rising memory use; a use-after-free threatens correctness and can cause unpredictable failures with little warning. Either can cause an outage.

**Damage in a Command-Line Tool that Exits in a Second**

When a process exits, the operating system reclaims its memory, so a small leak does not outlive that run. But exit limits how long a leak lasts, not how large it grows. A tool that leaks on every item of a very large input can still run out of memory before it exits. If the same code is reused in a long-running program, the leak can accumulate for much longer.

A short run does not make a use-after-free safe. What matters is whether execution reaches the faulty path. If it does, a stale read may produce incorrect output or data that is later written to a result file. A stale write may corrupt data directly, and either can contribute to a crash. If the tool handles untrusted input, an attacker may be able to steer execution toward that path.

**Limits of the protection:**

The checks protect only operations that go through them: `dt_ref_borrow` rejects borrowing after release, and `dt_ref_release` rejects a second release. Code that bypasses these functions, or uses a handle after `dt_ref_destroy` has freed it, is not protected.

The handle deliberately outlives the release. `dt_ref_release` frees `cell` but keeps `p`, so the `released` flag remains available. The handle stays allocated until `dt_ref_destroy` runs; in a long-running application, handles can accumulate if references are released but never destroyed. Destroying a handle is safe only once no other code can use it.

Aliasing is also outside the reference's ownership model. The comment in `dt_ref_new` says "Ownership stops at the cell": the reference owns its cell, not the objects a stored value may point to. A referenced string, for example, remains owned by the environment, so `dt_ref` cannot detect if that string is freed elsewhere. Also, `dt_ref_borrow(NULL)` returns `DT_ERR_RELEASED`, so a missing reference and a released reference produce the same status. This mechanism models a limited set of ownership errors. Hence, it is not a general memory-safety guarantee. 
