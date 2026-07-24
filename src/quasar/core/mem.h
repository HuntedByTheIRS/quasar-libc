#ifndef QUASAR_CORE_MEM_H
#define QUASAR_CORE_MEM_H

#include <stddef.h>
#include "quasar/std/memory.h"

/*
 * Alignment used for all Quasar allocations.
 * Matches max_align_t on typical 64-bit platforms (SSE, long double).
 */
#define MEM_ALIGN 16
#define MEM_ALIGN_UP(n) (((n) + MEM_ALIGN - 1) & ~(MEM_ALIGN - 1))

/* Default chunk size for chunked allocators. */
#define MEM_DEFAULT_CHUNK_SIZE 256

/*
 * Chunk node for chunked allocators (Dynamic, Arena, Temp, Leak).
 * Each chunk is a contiguous buffer allocated via malloc().
 * New chunks are appended as a linked list so existing pointers
 * remain stable across future allocations.
 */
typedef struct MemChunk {
	unsigned char   *data;
	size_t           size;
	size_t           used;
	struct MemChunk *next;
} MemChunk;

/*
 * Opaque allocator handle.
 *
 * MANUAL:        single contiguous buffer (m.data/m.size/m.used).
 *                Resizing may invalidate existing pointers.
 * All others:    chunked (c.head/c.current).
 *                Existing pointers are stable. Growth appends new chunks.
 */
struct Allocator {
	AllocatorType type;
	union {
		struct {
			unsigned char *data;
			size_t         size;
			size_t         used;
		} m;
		struct {
			MemChunk *head;
			MemChunk *current;
		} c;
	} u;
};

/*
 * Internal allocation from any allocator.  Returns a suitably-aligned
 * pointer, or NULL on failure / exhaustion.
 */
void *_quasar_mem_allocate(struct Allocator *a, size_t sz);

/*
 * Internal: free the allocator and all memory it owns.
 * Does NOT check the leak flag — caller must decide whether to skip.
 */
void _quasar_mem_allocator_free(struct Allocator *a);

#endif
