# Quasar Data Structures & Vectors Specification

This document defines the planned data structures and vector types provided by
Quasar.

These APIs are designed to provide common data structures and numerical
containers that are frequently reimplemented in C projects.

All data structures should integrate with Quasar's allocator system where
appropriate.

The implementation should remain explicit and predictable. Quasar does not
attempt to hide the underlying memory model or introduce automatic garbage
collection.

---

# Dynamic Arrays

Dynamic arrays provide contiguous, resizable storage for elements of a fixed
type.

Dynamic arrays are intended to provide a standard alternative to repeatedly
implementing `malloc`, `realloc`, capacity tracking, and length tracking in
individual projects.

Dynamic arrays grow automatically when additional capacity is required.

The array owns its allocated storage through the allocator used to create it.

## Goals

- Contiguous memory.
- O(1) indexed access.
- O(1) amortized append.
- Automatic growth.
- Explicit ownership.
- Allocator-backed storage.
- Ability to reserve capacity.
- Ability to shrink capacity when desired.
- Simple iteration.

## Core Operations

```c
array_t array_new(Allocator *a, size_t element_size);
array_t array_with_capacity(Allocator *a, size_t element_size, size_t capacity);

void array_free(array_t *array);

size_t array_len(array_t array);
size_t array_capacity(array_t array);
size_t array_element_size(array_t array);

void *array_at(array_t array, size_t index);
const void *array_at_const(array_t array, size_t index);

void array_push(array_t *array, const void *element);
void array_pop(array_t *array, void *out);

void array_insert(array_t *array, size_t index, const void *element);
void array_remove(array_t *array, size_t index, void *out);

void array_clear(array_t *array);

bool array_reserve(array_t *array, size_t capacity);
bool array_resize(array_t *array, size_t length);
bool array_shrink_to_fit(array_t *array);
```

The exact API may vary depending on whether Quasar implements generic
type-aware arrays or macro-generated typed arrays.

## Typed Dynamic Arrays

Where possible, Quasar should provide typed dynamic arrays that avoid
repeated casts and manual element-size management.

Example:

```c
vec_ti32 numbers = vec_i32_new(arena);

vec_i32_push(&numbers, 10);
vec_i32_push(&numbers, 20);
vec_i32_push(&numbers, 30);

int32_t value = vec_i32_at(numbers, 1);
```

Typed dynamic arrays should provide compile-time type information where the C
type system permits it.

---

# Hash Maps

Hash maps provide key-value storage with average O(1) lookup, insertion, and
removal.

Hash maps are intended for applications requiring fast associative lookup
without requiring each project to implement its own hashing and collision
handling.

## Goals

- Average O(1) lookup.
- Average O(1) insertion.
- Average O(1) removal.
- Explicit ownership.
- Allocator-backed storage.
- Support for common key types.
- User-defined hashing where appropriate.
- User-defined equality where appropriate.
- Predictable resizing behavior.

## Core Operations

```c
map_t map_new(Allocator *a);
map_t map_new_with_capacity(Allocator *a, size_t capacity);

void map_free(map_t *map);

size_t map_len(map_t map);
size_t map_capacity(map_t map);

bool map_insert(map_t *map, ...);
bool map_get(map_t map, ...);
bool map_contains(map_t map, ...);
bool map_remove(map_t *map, ...);

void map_clear(map_t *map);
bool map_reserve(map_t *map, size_t capacity);
```

The exact key and value interface should support both built-in and
user-defined types.

## Hashing

Quasar should provide standard hashing functions for common types.

Potential built-in hashing support includes:

```c
hash_i8()
hash_i16()
hash_i32()
hash_i64()

hash_u8()
hash_u16()
hash_u32()
hash_u64()

hash_f32()
hash_f64()

hash_str()
hash_bytes()
```

Users should be able to provide custom hash and equality functions for
user-defined keys.

Example:

```c
HashMap map = map_new(arena);

map_set(&map, "name", "Quasar");

const char *name = map_get(&map, "name");
```

The exact generic interface may differ depending on the final Quasar type
system.

---

# Linked Lists

Linked lists provide dynamically allocated nodes connected through pointers.

Linked lists are intended for cases where frequent insertion and removal of
elements is more important than contiguous memory access.

## Goals

- O(1) insertion and removal when a node is known.
- Explicit allocator ownership.
- No unnecessary individual allocation when an arena is used.
- Support for forward and bidirectional traversal.

Quasar should provide both singly linked and doubly linked lists where useful.

## Singly Linked Lists

```c
list_t list_new(Allocator *a);

void list_free(list_t *list);

size_t list_len(list_t list);

void list_push_front(list_t *list, ...);
void list_push_back(list_t *list, ...);

void list_pop_front(list_t *list, ...);
void list_pop_back(list_t *list, ...);

void list_insert(list_t *list, ...);
void list_remove(list_t *list, ...);

void list_clear(list_t *list);
```

## Doubly Linked Lists

Doubly linked lists should provide efficient traversal in both directions.

```c
dlist_t dlist_new(Allocator *a);

void dlist_push_front(dlist_t *list, ...);
void dlist_push_back(dlist_t *list, ...);

void dlist_pop_front(dlist_t *list, ...);
void dlist_pop_back(dlist_t *list, ...);

void dlist_insert_before(dlist_t *list, ...);
void dlist_insert_after(dlist_t *list, ...);

void dlist_remove(dlist_t *list, ...);
```

Linked lists should generally be used when their specific properties are
actually useful. They should not be treated as a general replacement for
dynamic arrays.

---

# Ring Buffers

Ring buffers provide fixed-capacity circular storage.

They are intended for queues, streaming data, producer-consumer systems,
logging, networking, audio, and other workloads where data is continuously
added and removed.

Ring buffers should avoid moving existing elements when data is pushed or
popped.

## Goals

- O(1) push.
- O(1) pop.
- O(1) access to front and back.
- No element shifting.
- Fixed or dynamically growing capacity.
- Allocator-backed storage.
- Efficient memory usage.

## Core Operations

```c
ring_t ring_new(Allocator *a, size_t element_size, size_t capacity);

void ring_free(ring_t *ring);

size_t ring_len(ring_t ring);
size_t ring_capacity(ring_t ring);

bool ring_is_empty(ring_t ring);
bool ring_is_full(ring_t ring);

bool ring_push(ring_t *ring, const void *element);
bool ring_pop(ring_t *ring, void *out);

void *ring_front(ring_t ring);
void *ring_back(ring_t ring);

void ring_clear(ring_t *ring);
```

## Dynamic Ring Buffers

A dynamic ring buffer may grow when full.

When growing, elements should be reordered into a contiguous logical sequence
within the new allocation.

Existing element pointers may become invalid after growth.

```c
ring_t ring_new_dynamic(Allocator *a, size_t element_size);
```

Fixed-capacity ring buffers should never allocate additional memory after
creation.

---

# Vectors

Vectors provide SIMD-oriented numerical containers for common integer and
floating-point types.

Unlike dynamic arrays, vectors represent fixed-width SIMD values rather than
arbitrarily sized collections.

Quasar vectors should use the highest SIMD instruction set supported by the
target CPU when possible.

The vector API should provide a consistent abstraction over architecture-
specific SIMD implementations.

The user should not need to directly use compiler-specific vector extensions
such as:

```c
__attribute__((vector_size(...)))
```

or compiler-specific SIMD types.

Quasar should handle the underlying implementation.

---

# Dynamic Numerical Vectors

Dynamic numerical vectors are heap-allocated collections of numerical values.

These vectors are not necessarily SIMD vectors.

They are intended for general-purpose numerical data where the number of
elements is not known at compile time.

```c
vec_ti8
vec_ti16
vec_ti32
vec_ti64

vec_tf32
vec_tf64
```

These vectors should provide:

- Dynamic resizing.
- Contiguous storage.
- O(1) indexed access.
- Automatic capacity growth.
- Allocator-backed storage.

Example:

```c
vec_ti32 numbers = vec_i32_new(arena);

vec_i32_push(&numbers, 10);
vec_i32_push(&numbers, 20);
vec_i32_push(&numbers, 30);
```

The exact naming convention may be adjusted to maintain consistency with the
rest of the Quasar API.

---

# Generic Dynamic Vector

Quasar should provide a type-agnostic dynamic vector for arbitrary C types.

```c
vec_v
```

The generic vector is not SIMD-oriented.

It stores elements in heap-allocated contiguous memory and uses the element
size provided by the user.

Example:

```c
vec_v values = vec_new(arena, sizeof(MyStruct));

MyStruct item = { ... };

vec_push(&values, &item);
```

The generic vector should provide:

- Arbitrary element sizes.
- Dynamic capacity.
- Contiguous storage.
- Allocator-backed ownership.
- O(1) indexed access.
- O(1) amortized append.

---

# SIMD Vectors

SIMD vectors represent fixed-width hardware vector registers.

Quasar should expose common SIMD widths without requiring users to manually
write architecture-specific intrinsics.

The initial supported widths are:

- 128-bit
- 256-bit
- 512-bit

The number of elements depends on the element type and vector width.

## Integer Vectors

| Type | 128-bit | 256-bit | 512-bit |
|------|---------|---------|---------|
| `i8` | `vec_16i8` | `vec_32i8` | `vec_64i8` |
| `i16` | `vec_8i16` | `vec_16i16` | `vec_32i16` |
| `i32` | `vec_4i32` | `vec_8i32` | `vec_16i32` |
| `i64` | `vec_2i64` | `vec_4i64` | `vec_8i64` |

## Floating-Point Vectors

| Type | 128-bit | 256-bit | 512-bit |
|------|---------|---------|---------|
| `f32` | `vec_4f32` | `vec_8f32` | `vec_16f32` |
| `f64` | `vec_2f64` | `vec_4f64` | `vec_8f64` |

---

# SIMD Backend Selection

Quasar should select the highest usable SIMD implementation supported by the
current CPU.

For example:

```text
AVX-512
    ↓ unavailable
AVX2
    ↓ unavailable
SSE / SSE2
    ↓ unavailable
Scalar fallback
```

The public API should remain consistent regardless of the selected backend.

The user should not need to manually select the SIMD instruction set for
normal use.

Where runtime CPU feature detection is used, Quasar should select an
appropriate implementation without requiring the user to rewrite their code.

---

# SIMD Operations

SIMD vectors should provide operations appropriate to their underlying type.

Potential operations include:

## Arithmetic

```c
vec_add()
vec_sub()
vec_mul()
vec_div()
```

## Bitwise

```c
vec_and()
vec_or()
vec_xor()
vec_not()
```

## Comparison

```c
vec_eq()
vec_ne()
vec_lt()
vec_gt()
vec_le()
vec_ge()
```

## Reduction

```c
vec_sum()
vec_min()
vec_max()
```

## Memory

```c
vec_load()
vec_load_aligned()
vec_store()
vec_store_aligned()
```

## Utility

```c
vec_zero()
vec_set()
vec_broadcast()
```

The exact operation names should be consistent across vector types.

---

# SIMD Alignment

SIMD vectors may require alignment appropriate to their underlying
representation.

Quasar should ensure that vector types have valid alignment requirements.

For example:

```text
128-bit vector → 16-byte alignment
256-bit vector → 32-byte alignment
512-bit vector → 64-byte alignment
```

The implementation may use unaligned loads and stores where supported.

Users should not be required to manually calculate SIMD alignment for normal
vector operations.

---

# SIMD Portability

The SIMD API should abstract architecture-specific implementation details.

The initial implementation may target common x86-64 SIMD instruction sets.

Future implementations may support:

- x86 SSE
- x86 AVX
- x86 AVX2
- x86 AVX-512
- ARM NEON
- ARM SVE
- Other architectures where practical

The public API should not expose architecture-specific intrinsics unless
explicitly requested by the user.

When SIMD is unavailable, operations should have a valid fallback where
possible.

The scalar fallback should preserve the semantics of the SIMD implementation.

---

# Allocator Integration

All heap-backed Quasar data structures should integrate with the Quasar
allocator system.

For example:

```c
Allocator *arena = mem_allocators_arena();

str_t name = str_from(arena, "Quasar");

vec_ti32 values = vec_i32_new(arena);

HashMap map = map_new(arena);

ring_t queue = ring_new(arena, sizeof(int), 128);
```

When the allocator is destroyed, all objects owned by it become invalid.

This allows entire groups of related data structures to share a lifetime.

For example:

```c
Allocator *arena = mem_allocators_arena();

vec_ti32 numbers = vec_i32_new(arena);
HashMap map = map_new(arena);
str_t name = str_from(arena, "Quasar");

/* All objects share the arena lifetime. */

mem_allocator_free(arena);
```

No individual cleanup is required when the allocator strategy permits bulk
destruction.

---

# Design Principles

Quasar data structures should follow these principles:

- **Allocator-backed.** Memory ownership should be explicit.
- **Predictable.** Operations should have documented complexity.
- **Composable.** Structures should work together naturally.
- **Contiguous where useful.** Prefer cache-friendly layouts when appropriate.
- **No unnecessary abstraction.** The implementation should remain understandable.
- **Type-aware where possible.** Avoid forcing users to manually cast everything.
- **Portable.** Architecture-specific optimizations should remain behind the API.
- **Efficient by default.** Common operations should have sensible complexity.
- **Explicit over magical.** Quasar should not hide ownership or lifetime.
- **C-native.** APIs should feel natural to C programmers.

Quasar is intended to provide the data structures that C programmers
repeatedly implement, while leaving the underlying language and machine model
visible.

The goal is not to replace C's primitives.

The goal is to stop rewriting the same damn primitives in every project.
