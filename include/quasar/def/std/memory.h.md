# std/memory.h

<!--toc:start-->

* [std/memory.h](#stdmemoryh)

  * [Ownership](#ownership)
  * [Allocators `mem_allocators_*`](#allocators-memallocators)

    * [**Allocator**](#allocator)

      * [*Example*](#example)
    * [**Manual Allocator** `Allocator* mem_allocators_manual(size_t sz)`](#manual-allocator-allocator-memallocatorsmanualsizet-sz)

      * [*Example*](#example-1)
      * [**Children** `mem_allocators_manual_*`](#children-memallocatorsmanual)
    * [**Dynamic Allocator** `Allocator* mem_allocators_dynamic(void)`](#dynamic-allocator-allocator-memallocatorsdynamicvoid)

      * [*Example*](#example-2)
      * [**Children** `mem_allocators_dynamic_*`](#children-memallocatorsdynamic)
    * [**Arena Allocator** `Allocator* mem_allocators_arena(void)`](#arena-allocator-allocator-memallocatorsarenavoid)

      * [*Example*](#example-3)
      * [**Children** `mem_allocators_arena_*`](#children-memallocatorsarena)
    * [**Temporary Allocator** `Allocator* mem_allocators_temp(void)`](#temporary-allocator-allocator-memallocatorstempvoid)

      * [*Example*](#example-4)
      * [**Children** `mem_allocators_temp_*`](#children-memallocatorstemp)
    * [**Leak Allocator** `Allocator* mem_allocators_leak(void)`](#leak-allocator-allocator-memallocatorsleakvoid)

      * [*Example*](#example-5)
  * [Allocator Conversion `mem_convert_*`](#allocator-conversion-memconvert)

    * [Conversion Rules](#conversion-rules)
    * [Valid Conversions](#valid-conversions)

      * [*Example*](#example-6)
  * [Choosing an Allocator](#choosing-an-allocator)

    * [Manual](#manual)
    * [Dynamic](#dynamic)
    * [Arena](#arena)
    * [Temporary](#temporary)
    * [Leak](#leak)
  * [Allocator Lifetime](#allocator-lifetime)
  * [Allocator Ownership](#allocator-ownership)
  * [Object Lifetime](#object-lifetime)
  * [Compatibility with libc](#compatibility-with-libc)
  * [Design Goals](#design-goals)

<!--toc:end-->

`memory.h` is a part of the Quasar standard library intended to provide
higher-level alternatives to directly managing memory with low-level
`malloc`, `realloc`, and `free` calls.

Quasar allocators provide different memory-management and lifetime strategies
while retaining compatibility with normal C and libc allocation.

Quasar does not attempt to replace the underlying system allocator. Instead,
it provides higher-level abstractions for common allocation patterns so that
programs do not need to repeatedly implement their own allocator wrappers and
lifetime-management systems.

---

## Ownership

Allocators own the memory they allocate.

The lifetime of an allocated object is determined by the allocator used to
create it. Objects allocated through an allocator do not own the allocator that
created them.

When an allocator is destroyed or otherwise invalidates its memory, all objects
whose memory is no longer part of the allocator become invalid.

The exact lifetime rules depend on the allocator strategy.

---

## Allocators `mem_allocators_*`

### **Allocator**

`Allocator` is an opaque allocator structure.

Allocator instances are returned by allocator creation functions and are
referenced through `Allocator*` handles.

Users should not need to interact with the internal fields or implementation
details of an `Allocator`.

The allocator handle is passed to other Quasar functions that require memory
allocation.

For example:

#### *Example*

```c
Allocator* p = mem_allocators_dynamic();
Allocator* a = mem_allocators_manual(100);

str_t s = str_from(p, "Allocated String");
str_t t = str_from(a, "Also allocated, uses a different allocator.");

mem_allocator_free(a);
mem_allocator_free(p);

// s and t are no longer valid because their parent allocators were freed.
```

The allocator itself owns the memory used by `s` and `t`.

Neither string owns its allocator.

---

### **Manual Allocator** `Allocator* mem_allocators_manual(size_t sz)`

A manual allocator provides a fixed memory region that is managed as a single
unit.

It is intended for situations where the required memory size is known or can
be explicitly managed.

Individual objects allocated from a manual allocator cannot be freed
independently.

Memory allocated from a manual allocator remains occupied for the lifetime of
the allocation region unless the allocator is explicitly resized or destroyed.

The manual allocator is easier to use than directly managing a raw allocation
with `malloc()` because the allocator itself represents the owned memory
region and provides helper functions for managing its size.

#### *Example*

```c
size_t size = 5;

Allocator* p = mem_allocators_manual(size + 1); // allocates 6 bytes

str_t s = str_from(p, "Hello");
// The string uses memory owned by p.
// The extra byte provides space for the null terminator.

qio_prints_printnl(s);

mem_allocator_free(p);
// The allocator and all memory owned by it are now freed.
```

#### **Children** `mem_allocators_manual_*`

`mem_allocators_manual_realloc_set(Allocator* ptr, size_t sz)`
Sets the size of the allocator's memory region to `sz` bytes.

Existing memory is never relocated by resizing a manual allocator.

If the new size is smaller than the current size, objects whose memory falls
outside the new allocation range become invalid.

`mem_allocators_manual_realloc_add(Allocator* ptr, size_t sz)`
Increases the size of the allocator's memory region by `sz` bytes.

The resulting size is the allocator's previous size plus `sz`.

`mem_allocators_manual_realloc_sub(Allocator* ptr, size_t sz)`
Decreases the size of the allocator's memory region by `sz` bytes.

Objects whose memory falls outside the new allocation range are not individually
freed. Their memory is simply no longer considered part of the allocator, and
references to that memory become invalid.

`mem_allocator_free(Allocator* ptr)`
Frees the allocator and all memory owned by it.

Individual objects allocated through a manual allocator cannot be freed
independently.

Memory obtained from a manual allocator remains valid until:

1. The allocator is freed.
2. The allocator is resized such that the object's memory falls outside its
   current range.
3. The allocator is converted to a strategy whose semantics invalidate the
   object.

Manual allocators should generally not be resized below the memory currently
occupied by live objects. Doing so intentionally is considered an advanced
operation and should only be performed when the resulting invalidation is
understood and expected.

---

### **Dynamic Allocator** `Allocator* mem_allocators_dynamic(void)`

A dynamic allocator automatically grows as additional memory is requested.

Unlike a manual allocator, the user does not need to know the total amount of
memory required when creating the allocator.

The dynamic allocator manages its own capacity and grows as necessary.

The dynamic allocator is intended for allocations whose required size is
unknown or changing and whose lifetime is longer than temporary allocations.

The dynamic allocator may use `malloc()` and `realloc()` internally to manage
its backing memory.

#### *Example*

```c
Allocator* p = mem_allocators_dynamic();

str_t s = str_from(p, "This string can be allocated dynamically.");
str_t t = str_from(p, "The allocator can grow as needed.");

mem_allocator_free(p);

// s and t are no longer valid.
```

#### **Children** `mem_allocators_dynamic_*`

`mem_allocator_free(Allocator* ptr)`
Frees a dynamic allocator and all memory owned by it.

---

### **Arena Allocator** `Allocator* mem_allocators_arena(void)`

An arena allocator is intended for allocating multiple objects that share a
common lifetime.

Unlike a manual allocator, an arena is not intended to represent one fixed
allocation. An arena may grow as needed and may contain multiple allocations
with different sizes.

Objects allocated from an arena are not individually freed.

Instead, the entire arena or its current allocation state is cleared or
destroyed when the objects are no longer needed.

This makes arenas useful for data structures or groups of objects that can all
be destroyed at the same time.

#### *Example*

```c
Allocator* arena = mem_allocators_arena();

str_t a = str_from(arena, "First");
str_t b = str_from(arena, "Second");
str_t c = str_from(arena, "Third");

// a, b, and c all share the lifetime of arena.

mem_allocator_free(arena);

// a, b, and c are no longer valid.
```

#### **Children** `mem_allocators_arena_*`

`mem_allocators_arena_clear(Allocator* ptr)`
Clears all allocations currently owned by the arena without destroying the
allocator itself.

Objects allocated before the clear operation become invalid.

The allocator may reuse the memory for subsequent allocations.

`mem_allocators_arena_reset(Allocator* ptr)`
Resets the arena to its initial allocation state.

Existing allocations become invalid, and the arena's memory may be reused.

`mem_allocator_free(Allocator* ptr)`
Frees the arena and all memory owned by it.

---

### **Temporary Allocator** `Allocator* mem_allocators_temp(void)`

A temporary allocator is intended for short-lived allocations.

Temporary allocators are useful when a function or operation requires an
allocator but the allocated objects are only needed for a limited period of
time.

The temporary allocator reduces the need to manually free many short-lived
allocations individually.

Temporary allocations may be cleared together when the temporary operation is
complete.

The intended lifetime of temporary allocations is shorter than the lifetime of
normal long-term allocations.

#### *Example*

```c
Allocator* temp = mem_allocators_temp();

str_t a = str_from(temp, "Temporary String");
str_t b = str_from(temp, "Another Temporary String");

// Use a and b.

mem_allocators_temp_clear(temp);

// a and b are no longer valid.
```

Temporary allocators are intended for scratch memory and other data whose
lifetime can be grouped around a specific operation.

#### **Children** `mem_allocators_temp_*`

`mem_allocators_temp_clear(Allocator* ptr)`
Clears temporary allocations and allows the allocator's memory to be reused.

Objects allocated before the clear operation become invalid.

`mem_allocators_temp_reset(Allocator* ptr)`
Resets the temporary allocator to its initial state.

Existing temporary allocations become invalid.

`mem_allocator_free(Allocator* ptr)`
Frees the temporary allocator and all memory owned by it.

---

### **Leak Allocator** `Allocator* mem_allocators_leak(void)`

A leak allocator is intended for allocations that do not need to be explicitly
freed during the lifetime of the program.

Memory allocated through a leak allocator is intentionally allowed to remain
allocated until the process terminates.

This allocator is useful for small objects or data that are created once and
remain valid for the entire lifetime of the program.

The leak allocator may use a dynamic allocator internally.

#### *Example*

```c
Allocator* leak = mem_allocators_leak();

str_t s = str_from(
    leak,
    "This string will live for the entire program."
);

qio_prints_printnl(s);

// No explicit free is required.
```

Memory allocated by a leak allocator is reclaimed by the operating system when
the process terminates.

The leak allocator should generally be used only when the lifetime of the
allocated data is intentionally the same as the lifetime of the process.

---

## Allocator Conversion `mem_convert_*`

Allocator conversion changes how an existing allocator manages its memory.

A conversion does not move the allocator's existing memory or relocate existing
objects.

Instead, the allocator's management strategy is changed while retaining the
existing underlying memory whenever the conversion semantics allow it.

The availability of a conversion depends on the current allocator strategy and
the target strategy.

Not every allocator can be converted to every other allocator.

A conversion may change the lifetime semantics of existing allocations.

If a conversion would invalidate existing objects, those objects must no longer
be accessed after the conversion.

### Conversion Rules

Allocator conversions are intended to preserve the allocator's existing memory
whenever possible.

The following principles apply:

* Existing memory should not be relocated by conversion.
* Existing objects remain at their existing addresses when the conversion
  preserves their validity.
* The allocator's future allocation behavior may change after conversion.
* The allocator's lifetime and destruction semantics may change after
  conversion.
* Some conversions may invalidate existing objects.
* Invalid conversions should be rejected rather than silently producing an
  allocator with undefined semantics.

The exact conversion behavior of each allocator pair is defined by the
corresponding allocator implementation.

### Valid Conversions

The currently supported conversion model is:

```text
manual  -> dynamic
manual  -> leak
manual  -> arena

dynamic -> leak
dynamic -> arena

arena   -> leak

leak    -> manual

temp    -> dynamic
temp    -> arena
temp    -> leak
```

Conversions not listed above are invalid.

#### *Example*

```c
Allocator* p = mem_allocators_manual(100);

str_t s = str_from(p, "Allocated String");

p = mem_convert_allocator(p, MEM_ARENA);

// s remains associated with the same underlying memory.
// The allocator now follows arena allocation semantics.

mem_allocator_free(p);
```

If the conversion preserves the validity of existing allocations, objects
continue to reference the same underlying memory.

The allocator's management behavior is changed without requiring existing data
to be copied to a new allocation.

---

## Choosing an Allocator

Different allocation strategies are intended for different use cases.

### Manual

Use a manual allocator when:

* The required memory size is known or can be managed explicitly.
* Individual allocations do not need to be freed.
* The entire memory region can share one lifetime.
* A fixed or explicitly controlled memory region is useful.

A manual allocator is not intended to provide individual object deallocation.

Memory allocated through a manual allocator is permanently occupied until the
allocator itself is resized or destroyed.

If long-term objects need independent lifetimes, consider using a dynamic
allocator instead.

If many objects share a common lifetime, consider using an arena.

### Dynamic

Use a dynamic allocator when:

* The required amount of memory is not known in advance.
* The allocator needs to grow as memory is requested.
* Allocated objects need a longer lifetime.
* Memory usage is not naturally grouped into one fixed region.
* Individual allocations may need more flexible lifetime management.

Dynamic allocation is intended for general-purpose long-lived allocation where
the required size may change over time.

### Arena

Use an arena when:

* Many objects share a common lifetime.
* Individual object deallocation is unnecessary.
* The entire group of allocations can be destroyed together.
* Many related objects are created during a single operation or phase.

Arenas are especially useful for complex data structures or operations where
many objects are created and later discarded together.

### Temporary

Use a temporary allocator when:

* Allocations are short-lived.
* Memory is only required during a specific operation.
* Individual cleanup would be unnecessary or cumbersome.
* Many temporary objects can be discarded together.

Temporary allocation is intended for scratch memory and other short-lived data.

### Leak

Use a leak allocator when:

* Allocated data is intended to remain valid until process termination.
* Explicit cleanup is unnecessary.
* The amount of retained memory is small or otherwise controlled.
* The data is created once and remains valid for the lifetime of the program.

Leak allocation should be used intentionally.

---

## Allocator Lifetime

An allocator owns the memory allocated through it.

When an allocator is destroyed, all objects allocated through that allocator
become invalid unless the allocator has been converted to a strategy that
preserves their validity.

Objects should not be used after their owning allocator has been destroyed or
after an allocator operation has invalidated the memory containing them.

For example:

```c
Allocator* p = mem_allocators_dynamic();

str_t s = str_from(p, "Hello");

mem_allocator_free(p);

// s is now invalid.
```

The lifetime of an object is therefore tied to the lifetime and behavior of its
allocator rather than requiring every individual object to be manually freed.

Different allocator strategies provide different lifetime semantics:

```text
Manual
    Objects persist for the lifetime of the owned memory region.
    Individual objects cannot be freed.

Dynamic
    The allocator grows as needed.
    Objects remain valid until the allocator or their containing memory is
    invalidated.

Arena
    Objects share a common lifetime.
    Individual objects cannot be freed.

Temporary
    Objects remain valid until temporary memory is cleared or reset.

Leak
    Objects remain valid until process termination.
```

---

## Allocator Ownership

Allocators themselves are owned by the caller that creates them.

The caller is responsible for managing the allocator's lifetime according to its
allocation strategy.

Objects allocated through an allocator do not own the allocator that created
them.

For example:

```c
Allocator* p = mem_allocators_dynamic();

str_t s = str_from(p, "Hello");
```

`s` does not own `p`.

The caller remains responsible for managing the lifetime of `p`.

Freeing `p` invalidates `s`.

This ownership model allows higher-level Quasar objects to remain independent
of the implementation details of the allocator that provides their memory.

---

## Object Lifetime

Objects allocated by Quasar functions are generally owned by the allocator
provided to those functions.

For example:

```c
Allocator* arena = mem_allocators_arena();

str_t name = str_from(arena, "Quasar");
str_t description = str_from(
    arena,
    "A higher level standard library for C."
);
```

Both strings use memory owned by `arena`.

The strings do not own the arena and should not attempt to free its memory
independently.

When the arena is cleared or destroyed:

```c
mem_allocator_free(arena);
```

both strings become invalid.

This model allows higher-level Quasar APIs to share a common memory-management
interface without requiring each data structure to implement its own allocation
strategy.

A higher-level Quasar object may provide its own cleanup functions when
necessary, but those functions should respect the ownership rules of the
allocator used to create the object's memory.

---

## Compatibility with libc

Quasar allocators are not intended to replace the underlying system memory
allocator.

Quasar may use standard allocation primitives such as:

```c
malloc()
realloc()
free()
```

internally.

The purpose of Quasar's allocator system is to provide higher-level allocation
strategies and lifetime management on top of existing libc allocation
facilities.

Quasar does not prevent users from using `malloc()`, `realloc()`, or `free()`
directly.

Raw libc allocation may still be used when it is more appropriate for a
particular use case.

Quasar's allocator system is an optional higher-level alternative for common
allocation patterns.

Programs may freely mix normal libc allocation with Quasar allocation when
appropriate, provided that memory is released using the correct ownership
system.

Memory allocated with `malloc()` must be released with `free()`.

Memory allocated by a Quasar allocator must be managed according to the
semantics of that allocator.

---

## Design Goals

The memory system is designed around the following goals:

* Provide a consistent allocator interface.
* Make common allocation patterns easier to express.
* Reduce repetitive manual memory management.
* Make object lifetimes explicit.
* Allow different lifetime strategies to share a common interface.
* Allow allocator strategies to be converted when possible.
* Avoid reinventing the underlying system allocator.
* Remain compatible with normal C and libc allocation.
* Allow higher-level Quasar APIs to accept allocators without knowing their
  implementation.
* Keep memory management explicit without requiring every allocation to be
  manually managed.
* Provide sensible defaults for common allocation patterns.
* Allow programmers to choose the memory lifetime model that best fits their
  application.

The allocator system is intended to be used throughout Quasar.

Higher-level APIs such as strings, collections, and other data structures may
accept an `Allocator*` so that users can control how the memory used by those
objects is managed.

For example:

```c
Allocator* arena = mem_allocators_arena();

str_t name = str_from(arena, "Quasar");
str_t description = str_from(
    arena,
    "A higher level standard library for C."
);

// Both strings share the lifetime of the arena.

mem_allocator_free(arena);
```

This allows memory management to remain explicit while moving the complexity of
allocation strategy out of individual high-level data structures.

Quasar does not attempt to eliminate the underlying C memory model.

Instead, it provides a consistent and reusable abstraction around it.

The goal is to make common memory-management patterns less repetitive, more
predictable, and easier to reason about without taking away the programmer's
ability to use libc directly when necessary.
