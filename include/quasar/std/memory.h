#ifndef QUASAR_STD_MEMORY_H
#define QUASAR_STD_MEMORY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Allocator type tags.  Each tag selects a distinct allocation and
 * lifetime strategy.  The opaque Allocator* handle is passed to other
 * Quasar APIs that require memory allocation.
 */
typedef enum {
	MEM_MANUAL,
	MEM_DYNAMIC,
	MEM_ARENA,
	MEM_TEMP,
	MEM_LEAK,
} AllocatorType;

typedef struct Allocator Allocator;

/*
 * ── Creation ───────────────────────────────────────────────────────
 *
 * All creation functions return NULL on allocation failure.
 */

/*
 * Manual allocator: a single contiguous buffer of sz bytes.
 *
 * Pointer stability:
 *   Existing pointers MAY be invalidated by realloc_set, realloc_add,
 *   and realloc_sub because the underlying buffer is managed with
 *   realloc().  Resizing is explicit; the allocator does not grow
 *   automatically.
 *
 * Individual objects allocated from a manual allocator cannot be freed
 * independently.
 *
 * mem_allocators_manual(0) creates a valid empty allocator that can be
 * grown later via realloc_set.
 */
Allocator *mem_allocators_manual(size_t sz);

/*
 * Dynamic allocator: a growable bump allocator.  When the current
 * backing chunk is exhausted, a new chunk is appended automatically.
 *
 * Pointer stability:
 *   Existing allocations are STABLE.  Growing the allocator appends
 *   new chunks; old chunks are never relocated.
 *
 * Individual allocations cannot be freed independently.  All memory
 * is released when the allocator is freed.
 */
Allocator *mem_allocators_dynamic(void);

/*
 * Arena allocator: intended for groups of objects that share a common
 * lifetime.  Internally identical to Dynamic, but exposes clear/reset
 * operations for bulk lifetime management.
 *
 * Pointer stability:
 *   Existing allocations are STABLE until clear() or reset() is called.
 *   After clear() or reset(), all previously returned pointers are
 *   invalid.
 *
 * Individual allocations cannot be freed independently.
 */
Allocator *mem_allocators_arena(void);

/*
 * Temporary allocator: intended for short-lived scratch allocations.
 * Internally identical to Arena.  The distinction is semantic: Temp
 * signals that allocations are transient and should be cleared or
 * reset when the operation completes.
 *
 * Pointer stability:
 *   Existing allocations are STABLE until clear() or reset() is called.
 *   After clear() or reset(), all previously returned pointers are
 *   invalid.
 */
Allocator *mem_allocators_temp(void);

/*
 * Leak allocator: memory allocated through a leak allocator is
 * intentionally retained until process termination.  mem_allocator_free()
 * is a no-op for leak allocators — neither the allocator metadata nor
 * its backing memory is reclaimed.
 *
 * Pointer stability:
 *   Existing allocations are STABLE for the lifetime of the process.
 *
 * Use this only for data that is genuinely process-lifetime.
 */
Allocator *mem_allocators_leak(void);

/*
 * ── Manual resize ──────────────────────────────────────────────────
 *
 * These operations apply only to manual allocators.  They are no-ops
 * when passed any other allocator type or NULL.
 *
 * WARNING: resizing a manual allocator may relocate the underlying
 * buffer (via realloc), invalidating all previously returned pointers.
 * realloc() guarantees only max_align_t alignment (typically 16 bytes
 * on 64-bit platforms); resized buffers may not preserve the stricter
 * 16-byte alignment provided at creation time via aligned_alloc().
 * Shrinking below the current used offset clamps used to the new size,
 * permanently invalidating objects beyond the new boundary.
 *
 * Failure is silent: if realloc() fails, the allocator is left
 * unchanged.  Consider this when requesting large growth.
 */

/* Set the manual allocator's capacity to exactly sz bytes. */
void mem_allocators_manual_realloc_set(Allocator *ptr, size_t sz);

/* Increase capacity by sz bytes.  No-op if sz would cause overflow. */
void mem_allocators_manual_realloc_add(Allocator *ptr, size_t sz);

/* Decrease capacity by sz bytes.  If sz >= current size, frees the
   backing buffer entirely (equivalent to realloc_set(ptr, 0)). */
void mem_allocators_manual_realloc_sub(Allocator *ptr, size_t sz);

/*
 * ── Arena / Temp operations ─────────────────────────────────────────
 *
 * These operations are no-ops when passed NULL or the wrong allocator
 * type.
 */

/*
 * clear: reset the used counter to zero on every chunk in the chain.
 * All chunk memory is retained and immediately reusable.  The current
 * chunk pointer is reset to the head of the chain so that subsequent
 * allocations start from the first chunk.
 *
 * All pointers returned before the clear become invalid.
 */
void mem_allocators_arena_clear(Allocator *ptr);
void mem_allocators_temp_clear(Allocator *ptr);

/*
 * reset: free all chunks except the first, then reset the first
 * chunk's used counter to zero.  After reset, the allocator is back
 * to its initial state (one empty chunk).
 *
 * All pointers returned before the reset become invalid.
 */
void mem_allocators_arena_reset(Allocator *ptr);
void mem_allocators_temp_reset(Allocator *ptr);

/*
 * ── Destruction ────────────────────────────────────────────────────
 *
 * mem_allocator_free releases all memory owned by the allocator and
 * the allocator handle itself.  All pointers into allocator-owned
 * memory become invalid.
 *
 * For leak allocators, mem_allocator_free is a no-op: neither the
 * backing memory nor the allocator metadata is freed.  Memory is
 * reclaimed by the OS at process termination.
 */
void mem_allocator_free(Allocator *ptr);

/*
 * ── Conversion ─────────────────────────────────────────────────────
 *
 * Conversion changes an allocator's strategy/type without relocating
 * existing data whenever possible.  If a conversion is invalid or
 * fails due to memory exhaustion, the original allocator is returned
 * unchanged (the return value equals the input pointer).  The caller
 * retains a valid, freeable handle in all cases.
 *
 * Valid conversions and their pointer-stability guarantees:
 *
 *   Manual → Dynamic   pointers STABLE (buffer becomes first chunk)
 *   Manual → Arena     pointers STABLE (buffer becomes first chunk)
 *   Manual → Leak      pointers STABLE (buffer becomes first chunk)
 *
 *   Dynamic → Arena    pointers STABLE (chunked state preserved,
 *                       type tag only; clear/reset become available)
 *   Dynamic → Leak     pointers STABLE (type tag only; free becomes
 *                       no-op, memory leaks intentionally)
 *
 *   Arena → Leak       pointers STABLE (type tag only; clear/reset
 *                       become unavailable, free becomes no-op)
 *
 *   Temp → Dynamic     pointers STABLE (type tag only; clear/reset
 *                       become unavailable)
 *   Temp → Arena       pointers STABLE (type tag only)
 *   Temp → Leak        pointers STABLE (type tag only; free becomes
 *                       no-op)
 *
 *   Leak → Manual      0 or 1 chunk: pointers STABLE (chunk unwrapped)
 *                       N chunks:     pointers INVALIDATED (data is
 *                                     consolidated into a new buffer)
 *
 * All other conversion pairs are invalid.  The original allocator is
 * returned unchanged so the caller retains a valid, freeable handle.
 *
 * Conversions are additive: converting to a less-restrictive type
 * (e.g. Dynamic → Arena) adds capabilities (clear/reset).  Converting
 * to a more-restrictive type (e.g. Arena → Leak) removes capabilities
 * (clear/reset become unavailable, free becomes no-op).
 */
Allocator *mem_convert_allocator(Allocator *ptr, AllocatorType target);

#ifdef __cplusplus
}
#endif

#endif
