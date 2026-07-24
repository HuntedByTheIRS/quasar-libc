# Quasar — The Loveable LibC

> **Add the things to libc that you wish libc already had.**

Quasar is a C library that sits on top of [musl](https://musl.libc.org/), providing the higher-level utilities C programmers keep rewriting from scratch — strings, memory management, data structures, and more — with consistent APIs, explicit ownership, and no hidden magic.

```
    Your Application
          │
          ▼
       Quasar        ← you are here
          │
          ▼
        musl         ← reliable, portable libc
          │
          ▼
    Linux / POSIX
```

---

## Why?

Every C project eventually builds its own string type. Its own allocator wrappers. Its own path utilities. Each one slightly different, each one battle-tested in isolation, each one a distraction from the actual problem the project set out to solve.

Quasar says: **write it once, well, and move on.**

The goal is not to replace C, hide C, or paper over the system. It's to make the parts of C that programmers repeatedly complain about less of a chore — without making the underlying machine mysterious.

---

## What's Implemented

| Module | Status |
|--------|--------|
| `std/memory` — allocator abstraction (manual, dynamic, arena, temp, leak) | ✅ Implemented & tested |
| `std/strings` — allocator-backed string type | 🚧 Planned |
| Everything else | 📋 Roadmap |

### Memory system — done

Five allocator strategies, one consistent interface:

```c
#include <quasar/std/memory.h>

// Choose your lifetime strategy
Allocator *arena = mem_allocators_arena();

// Pass it to any Quasar API — the allocator owns the memory
str_t name = str_from(arena, "Quasar");

// Bulk-free everything when you're done
mem_allocator_free(arena);
```

| Allocator | Use when |
|-----------|----------|
| **Manual** | You know the size upfront, want explicit control |
| **Dynamic** | Size is unknown, allocator grows as needed |
| **Arena** | Many objects share a lifetime — free them all at once |
| **Temp** | Scratch memory for a single operation |
| **Leak** | Data lives until process exit, no cleanup needed |

Allocators can be **converted** between strategies without moving data — change how memory is managed mid-flight:

```c
Allocator *a = mem_allocators_manual(256);
// ... allocate objects ...
a = mem_convert_allocator(a, MEM_ARENA);  // now has clear/reset
```

---

## Structure

```
quasar/
├── include/quasar/
│   ├── def/          ← specifications (what should exist)
│   ├── core/         ← internal implementation helpers
│   ├── std/          ← public API (what you use)
│   └── ext/          ← specialized / niche functionality
└── src/quasar/
    ├── core/         ← implementation of core internals
    └── std/          ← implementation of std modules
```

The public API uses consistent `prefix_*` namespacing — no C++-style namespaces needed:

```
mem_*        str_*        math_*        proc_*
file_*       path_*       time_*        env_*
```

---

## Roadmap

### Phase 1 — Foundation ✅

- [x] Allocator system (`std/memory`) with 5 strategies + conversions
- [x] Pointer-stable chunked allocation
- [x] 16-byte aligned allocation
- [x] Comprehensive test suite (56 tests)

### Phase 2 — Strings

- [ ] `str_t` — allocator-backed string type
- [ ] Creation: `str_from()`, `str_from_len()`, `str_dup()`, `str_fmt()`
- [ ] Inspection: `str_len()`, `str_is_empty()`, `str_starts_with()`, `str_contains()`
- [ ] Comparison: `str_equals()`, `str_equals_icase()`
- [ ] Mutation: `str_cat()`, `str_insert()`, `str_remove()`, `str_replace()`
- [ ] Search: `str_index_of()`, `str_count()`
- [ ] Transform: `str_trim()`, `str_to_lower()`, `str_to_upper()`
- [ ] Split/join: `str_split()`, `str_join()`
- [ ] Slice: `str_slice()`, `str_substr()`

### Phase 3 — Data Structures

- [ ] Dynamic arrays
- [ ] Hash maps
- [ ] Linked lists
- [ ] Ring buffers

### Phase 4 — System

- [ ] Process management (`proc_*`)
- [ ] Filesystem operations (`file_*`, `path_*`)
- [ ] Time utilities (`time_*`)
- [ ] Environment (`env_*`)

### Phase 5 — Beyond

- [ ] Math utilities (`math_*`)
- [ ] I/O abstractions
- [ ] Networking
- [ ] Serialization
- [ ] Compression
- [ ] Randomness

---

## Building

Quasar is built as part of musl. From the project root:

```sh
# Configure for your architecture
echo "ARCH = x86_64" > config.mak

# Build
make

# Quasar headers land in include/quasar/
# Quasar objects land in obj/src/quasar/
```

Individual modules can be compiled standalone:

```sh
gcc -std=c11 -I include -c src/quasar/std/memory.c
```

---

## Design Principles

- **Explicit over implicit.** Memory ownership is visible. Lifetimes are clear.
- **Composable over monolithic.** Allocators plug into strings, strings plug into paths, paths plug into files.
- **Close to the metal.** You can always drop down to raw `malloc` / `free` when you need to.
- **No magic.** If you read the source, you understand what's happening.
- **Built on musl.** Benefits from musl's correctness, portability, and small footprint. Tracks upstream without heavy forking.

---

## License

Quasar (`src/quasar/`, `include/quasar/`) is licensed under the BSD 3-Clause license.  
The underlying musl libc remains under its original MIT license.

See [COPYRIGHT](COPYRIGHT) for full terms.

---

> **If C programmers constantly find themselves wishing libc had something, Quasar should consider providing it.**
>
> Not by replacing C. Not by hiding the system.
> Just by providing the damn utilities we keep writing over and over again.
