#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../core/mem.h"

/*
 * Allocate sz bytes aligned to MEM_ALIGN.  Rounds sz up to a
 * multiple of the alignment (required by aligned_alloc).
 */
static void *mem_aligned_alloc(size_t sz)
{
	sz = MEM_ALIGN_UP(sz);
	if (sz == 0) return 0;
	return aligned_alloc(MEM_ALIGN, sz);
}

/* ------------------------------------------------------------------ */
/*  Chunk helpers                                                      */
/* ------------------------------------------------------------------ */

static MemChunk *mem_chunk_alloc(size_t sz)
{
	MemChunk *c;

	c = malloc(sizeof(*c));
	if (!c) return 0;
	c->data = mem_aligned_alloc(sz);
	if (!c->data) {
		free(c);
		return 0;
	}
	c->size = sz;
	c->used = 0;
	c->next = 0;
	return c;
}

static void mem_chunk_free_chain(MemChunk *head)
{
	MemChunk *next;

	while (head) {
		next = head->next;
		free(head->data);
		free(head);
		head = next;
	}
}

static void mem_chunk_clear_chain(MemChunk *head)
{
	while (head) {
		head->used = 0;
		head = head->next;
	}
}

/*
 * Reset chunk chain: free all chunks after the first, reset the first
 * chunk's used counter to zero so its memory can be reused.
 */
static void mem_chunk_reset_chain(struct Allocator *a)
{
	MemChunk *head;
	MemChunk *tail;

	head = a->u.c.head;
	if (!head) return;
	tail = head->next;
	head->next = 0;
	mem_chunk_free_chain(tail);
	head->used = 0;
	a->u.c.current = head;
}

/* ------------------------------------------------------------------ */
/*  Core allocation                                                    */
/* ------------------------------------------------------------------ */

void *_quasar_mem_allocate(struct Allocator *a, size_t sz)
{
	unsigned char *p;

	if (!a) return 0;

	/* Treat zero-size as 1 byte so the returned pointer is unique. */
	if (sz == 0) sz = 1;

	if (a->type == MEM_MANUAL) {
		size_t offset;

		if (!a->u.m.data) return 0;

		offset = MEM_ALIGN_UP(a->u.m.used);
		if (offset < a->u.m.used) return 0;
		if (offset > a->u.m.size) return 0;
		if (sz > a->u.m.size - offset) return 0;

		p = a->u.m.data + offset;
		a->u.m.used = offset + sz;
		return p;
	}

	/* ---- Chunked allocators (Dynamic, Arena, Temp, Leak) ---- */
	/*
	 * Align the allocation's start address by rounding up the
	 * current chunk offset, rather than rounding the allocation
	 * size itself.  This avoids wasting MEM_ALIGN bytes on every
	 * tiny allocation; only the inter-allocation gap is padded.
	 */
	if (sz > SIZE_MAX - (MEM_ALIGN - 1)) return 0;

	for (;;) {
		MemChunk *c;
		MemChunk *prev;
		size_t    offset;

		/*
		 * Walk the chunk chain forward from current, trying
		 * each chunk.  After clear(), all chunks have used==0
		 * but only the head was reachable via current — this
		 * walk finds room in any existing chunk before
		 * allocating a new one.
		 */
		c    = a->u.c.current;
		prev = 0;
		while (c) {
			offset = MEM_ALIGN_UP(c->used);
			if (offset < c->used) { prev = c; c = c->next; continue; }
			if (offset <= c->size && sz <= c->size - offset) {
				p = c->data + offset;
				c->used = offset + sz;
				a->u.c.current = c;
				return p;
			}
			prev = c;
			c    = c->next;
		}
		{
			size_t    chunk_sz;
			size_t    fallback_sz;
			MemChunk *new_c;

			chunk_sz = MEM_DEFAULT_CHUNK_SIZE;
			if (prev && prev->size < SIZE_MAX / 2)
				chunk_sz = prev->size * 2;
			if (chunk_sz < sz)
				chunk_sz = sz;

			new_c = mem_chunk_alloc(chunk_sz);

			/*
			 * If the doubled-size allocation failed, retry
			 * with the exact required size.  This avoids
			 * premature OOM when the previous chunk has
			 * grown very large but the current request is
			 * small.
			 */
			if (!new_c) {
				fallback_sz = (sz > MEM_DEFAULT_CHUNK_SIZE)
				    ? sz : MEM_DEFAULT_CHUNK_SIZE;
				if (fallback_sz != chunk_sz)
					new_c = mem_chunk_alloc(fallback_sz);
			}
			if (!new_c) return 0;

			if (prev)
				prev->next = new_c;
			else
				a->u.c.head = new_c;
			a->u.c.current = new_c;
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Internal allocator free (both manual + chunked)                     */
/* ------------------------------------------------------------------ */

void _quasar_mem_allocator_free(struct Allocator *a)
{
	if (!a) return;

	if (a->type == MEM_MANUAL) {
		free(a->u.m.data);
	} else {
		mem_chunk_free_chain(a->u.c.head);
	}
	free(a);
}

/* ------------------------------------------------------------------ */
/*  Conversion helpers                                                  */
/* ------------------------------------------------------------------ */

static int mem_convert_is_valid(AllocatorType from, AllocatorType to)
{
	switch (from) {
	case MEM_MANUAL:
		return to == MEM_DYNAMIC || to == MEM_LEAK
		    || to == MEM_ARENA;
	case MEM_DYNAMIC:
		return to == MEM_LEAK || to == MEM_ARENA;
	case MEM_ARENA:
		return to == MEM_LEAK;
	case MEM_LEAK:
		return to == MEM_MANUAL;
	case MEM_TEMP:
		return to == MEM_DYNAMIC || to == MEM_ARENA
		    || to == MEM_LEAK;
	}
	return 0;
}

/*
 * Convert a manual allocator to a chunked allocator.
 * The existing single buffer becomes the first chunk.
 * Pointers into the original buffer remain valid.
 * Returns the (now converted) allocator on success, NULL on failure.
 */
static struct Allocator *mem_convert_manual_to_chunked(struct Allocator *a)
{
	MemChunk *c;

	if (a->u.m.data && a->u.m.size > 0) {
		c = malloc(sizeof(*c));
		if (!c) return 0;
		c->data = a->u.m.data;
		c->size = a->u.m.size;
		c->used = a->u.m.used;
		c->next = 0;
		a->u.c.head    = c;
		a->u.c.current = c;
	} else {
		a->u.c.head    = 0;
		a->u.c.current = 0;
	}
	return a;
}

/*
 * Convert a chunked allocator back to a manual (contiguous) allocator.
 *
 * - 0 chunks   → empty manual.
 * - 1 chunk    → the chunk's buffer is unwrapped directly (no copy).
 * - N chunks   → data is consolidated into a single malloc'd buffer.
 *                OLD POINTERS ARE INVALIDATED.
 */
static struct Allocator *mem_convert_chunked_to_manual(struct Allocator *a)
{
	MemChunk *head;
	MemChunk *c;

	head = a->u.c.head;

	if (!head) {
		a->u.m.data = 0;
		a->u.m.size = 0;
		a->u.m.used = 0;
		return a;
	}

	if (!head->next) {
		/* Single chunk — unwrap it without copying. */
		a->u.m.data = head->data;
		a->u.m.size = head->size;
		a->u.m.used = head->used;
		free(head);
		return a;
	}

	/* Multiple chunks — consolidate into a single contiguous buffer. */
	{
		size_t total;
		size_t offset;
		unsigned char *buf;

		total = 0;
		for (c = head; c; c = c->next) {
			if (c->used > SIZE_MAX - total)
				return 0;
			total += c->used;
		}

		if (total == 0) {
			mem_chunk_free_chain(head);
			a->u.m.data = 0;
			a->u.m.size = 0;
			a->u.m.used = 0;
			return a;
		}

		buf = mem_aligned_alloc(total);
		if (!buf) return 0;

		offset = 0;
		for (c = head; c; c = c->next) {
			if (c->used) {
				memcpy(buf + offset, c->data, c->used);
				offset += c->used;
			}
		}
		mem_chunk_free_chain(head);
		a->u.m.data = buf;
		a->u.m.size = total;
		a->u.m.used = total;
	}
	return a;
}

/* ------------------------------------------------------------------ */
/*  Public API — creation                                               */
/* ------------------------------------------------------------------ */

static struct Allocator *mem_allocator_new(AllocatorType type)
{
	struct Allocator *a;

	a = calloc(1, sizeof(*a));
	if (!a) return 0;
	a->type = type;
	return a;
}

Allocator *mem_allocators_manual(size_t sz)
{
	struct Allocator *a;

	a = mem_allocator_new(MEM_MANUAL);
	if (!a) return 0;
	if (sz > 0) {
		a->u.m.data = mem_aligned_alloc(sz);
		if (!a->u.m.data) {
			free(a);
			return 0;
		}
		a->u.m.size = sz;
	}
	return (Allocator *)a;
}

Allocator *mem_allocators_dynamic(void)
{
	return (Allocator *)mem_allocator_new(MEM_DYNAMIC);
}

Allocator *mem_allocators_arena(void)
{
	return (Allocator *)mem_allocator_new(MEM_ARENA);
}

Allocator *mem_allocators_temp(void)
{
	return (Allocator *)mem_allocator_new(MEM_TEMP);
}

Allocator *mem_allocators_leak(void)
{
	return (Allocator *)mem_allocator_new(MEM_LEAK);
}

/* ------------------------------------------------------------------ */
/*  Public API — manual resize                                          */
/* ------------------------------------------------------------------ */

void mem_allocators_manual_realloc_set(Allocator *ptr, size_t sz)
{
	struct Allocator *a;

	if (!ptr) return;
	a = (struct Allocator *)ptr;
	if (a->type != MEM_MANUAL) return;

	if (sz == 0) {
		free(a->u.m.data);
		a->u.m.data = 0;
		a->u.m.size = 0;
		a->u.m.used = 0;
		return;
	}

	{
		unsigned char *new_data;

		new_data = realloc(a->u.m.data, sz);
		if (!new_data) return;
		a->u.m.data = new_data;
		a->u.m.size = sz;
		if (a->u.m.used > sz)
			a->u.m.used = sz;
	}
}

void mem_allocators_manual_realloc_add(Allocator *ptr, size_t sz)
{
	struct Allocator *a;

	if (!ptr) return;
	a = (struct Allocator *)ptr;
	if (a->type != MEM_MANUAL) return;
	if (sz > SIZE_MAX - a->u.m.size) return; /* would overflow */
	mem_allocators_manual_realloc_set(ptr, a->u.m.size + sz);
}

void mem_allocators_manual_realloc_sub(Allocator *ptr, size_t sz)
{
	struct Allocator *a;

	if (!ptr) return;
	a = (struct Allocator *)ptr;
	if (a->type != MEM_MANUAL) return;
	if (sz >= a->u.m.size)
		mem_allocators_manual_realloc_set(ptr, 0);
	else
		mem_allocators_manual_realloc_set(ptr, a->u.m.size - sz);
}

/* ------------------------------------------------------------------ */
/*  Public API — arena operations                                       */
/* ------------------------------------------------------------------ */

void mem_allocators_arena_clear(Allocator *ptr)
{
	struct Allocator *a;

	if (!ptr) return;
	a = (struct Allocator *)ptr;
	if (a->type != MEM_ARENA) return;
	mem_chunk_clear_chain(a->u.c.head);
	a->u.c.current = a->u.c.head;
}

void mem_allocators_arena_reset(Allocator *ptr)
{
	struct Allocator *a;

	if (!ptr) return;
	a = (struct Allocator *)ptr;
	if (a->type != MEM_ARENA) return;
	mem_chunk_reset_chain(a);
}

/* ------------------------------------------------------------------ */
/*  Public API — temp operations                                        */
/* ------------------------------------------------------------------ */

void mem_allocators_temp_clear(Allocator *ptr)
{
	struct Allocator *a;

	if (!ptr) return;
	a = (struct Allocator *)ptr;
	if (a->type != MEM_TEMP) return;
	mem_chunk_clear_chain(a->u.c.head);
	a->u.c.current = a->u.c.head;
}

void mem_allocators_temp_reset(Allocator *ptr)
{
	struct Allocator *a;

	if (!ptr) return;
	a = (struct Allocator *)ptr;
	if (a->type != MEM_TEMP) return;
	mem_chunk_reset_chain(a);
}

/* ------------------------------------------------------------------ */
/*  Public API — free                                                   */
/* ------------------------------------------------------------------ */

void mem_allocator_free(Allocator *ptr)
{
	struct Allocator *a;

	if (!ptr) return;
	a = (struct Allocator *)ptr;
	if (a->type == MEM_LEAK)
		return;
	_quasar_mem_allocator_free(a);
}

/* ------------------------------------------------------------------ */
/*  Public API — conversion                                             */
/* ------------------------------------------------------------------ */

Allocator *mem_convert_allocator(Allocator *ptr, AllocatorType target)
{
	struct Allocator *a;
	AllocatorType from;

	if (!ptr) return 0;
	a = (struct Allocator *)ptr;
	from = a->type;

	/* Trivial same-type conversion. */
	if (from == target)
		return ptr;

	if (!mem_convert_is_valid(from, target))
		return ptr;

	/* manual → chunked (Dynamic, Arena, Leak). */
	if (from == MEM_MANUAL) {
		if (!mem_convert_manual_to_chunked(a))
			return ptr;
		a->type = target;
		return ptr;
	}

	/* chunked → manual (Leak → Manual). */
	if (target == MEM_MANUAL) {
		if (!mem_convert_chunked_to_manual(a))
			return ptr;
		a->type = MEM_MANUAL;
		return ptr;
	}

	/* chunked ↔ chunked — only the type tag changes. */
	a->type = target;
	return ptr;
}
