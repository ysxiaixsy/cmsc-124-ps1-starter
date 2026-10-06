# ANALYSIS.md

## 1. Three categories and what their languages pay

### 1a. [type, eg. Arrays]: [language] `[dt]`

<!-- Lau, jus remove this if ma paste kna -->

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

<!-- Lau, also this-->
