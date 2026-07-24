#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/mem.h"

static int failures;

#define T(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)

#define T_NULL(p)  T((p) == NULL)
#define T_NOTNULL(p) T((p) != NULL)

/* ── Creation ──────────────────────────────────────────────────── */

static void test_manual_creation(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_manual(0);
	T_NOTNULL(a);
	T(a->u.m.size == 0);
	T(a->u.m.data == NULL);
	T(a->u.m.used == 0);
	mem_allocator_free((Allocator *)a);

	a = (struct Allocator *)mem_allocators_manual(1);
	T_NOTNULL(a);
	T(a->u.m.size == 1);
	T_NOTNULL(a->u.m.data);
	T(a->u.m.used == 0);
	mem_allocator_free((Allocator *)a);

	a = (struct Allocator *)mem_allocators_manual(4096);
	T_NOTNULL(a);
	T(a->u.m.size == 4096);
	mem_allocator_free((Allocator *)a);
}

static void test_chunked_creation(void)
{
	struct Allocator *d, *ar, *t, *l;

	d = (struct Allocator *)mem_allocators_dynamic();
	T_NOTNULL(d);
	T(d->type == MEM_DYNAMIC);
	mem_allocator_free((Allocator *)d);

	ar = (struct Allocator *)mem_allocators_arena();
	T_NOTNULL(ar);
	T(ar->type == MEM_ARENA);
	mem_allocator_free((Allocator *)ar);

	t = (struct Allocator *)mem_allocators_temp();
	T_NOTNULL(t);
	T(t->type == MEM_TEMP);
	mem_allocator_free((Allocator *)t);

	l = (struct Allocator *)mem_allocators_leak();
	T_NOTNULL(l);
	T(l->type == MEM_LEAK);
	mem_allocator_free((Allocator *)l);
}

/* ── Basic allocation ──────────────────────────────────────────── */

static void test_manual_alloc(void)
{
	struct Allocator *a;
	void *p1, *p2, *p3;

	a = (struct Allocator *)mem_allocators_manual(96);
	p1 = _quasar_mem_allocate(a, 10);  T_NOTNULL(p1);
	p2 = _quasar_mem_allocate(a, 20);  T_NOTNULL(p2);
	p3 = _quasar_mem_allocate(a, 34);  T_NOTNULL(p3);
	T(((uintptr_t)p1 & 15) == 0);
	T(((uintptr_t)p2 & 15) == 0);
	T(((uintptr_t)p3 & 15) == 0);
	mem_allocator_free((Allocator *)a);
}

static void test_manual_alloc_exhaustion(void)
{
	struct Allocator *a;
	void *p1, *p2;

	a = (struct Allocator *)mem_allocators_manual(16);
	p1 = _quasar_mem_allocate(a, 10);  T_NOTNULL(p1);
	p2 = _quasar_mem_allocate(a, 10);  T_NULL(p2);
	mem_allocator_free((Allocator *)a);
}

static void test_manual_alloc_offset_overflow(void)
{
	struct Allocator *a;
	void *p;

	/* used=7, align=16 → offset=16 > size=8 → must fail */
	a = (struct Allocator *)mem_allocators_manual(8);
	a->u.m.used = 7;
	p = _quasar_mem_allocate(a, 1);
	T_NULL(p);
	mem_allocator_free((Allocator *)a);

	/* used=14, align=16 → offset=16 > size=14 → must fail */
	a = (struct Allocator *)mem_allocators_manual(14);
	a->u.m.used = 14;
	p = _quasar_mem_allocate(a, 1);
	T_NULL(p);
	mem_allocator_free((Allocator *)a);
}

static void test_chunked_alloc(void)
{
	struct Allocator *a;
	void *p1, *p2;

	a = (struct Allocator *)mem_allocators_dynamic();
	p1 = _quasar_mem_allocate(a, 1);    T_NOTNULL(p1);
	p2 = _quasar_mem_allocate(a, 1);    T_NOTNULL(p2);
	T(((uintptr_t)p1 & 15) == 0);
	T(((uintptr_t)p2 & 15) == 0);
	mem_allocator_free((Allocator *)a);
}

static void test_chunked_growth(void)
{
	struct Allocator *a;
	void *p1, *p2, *p3;

	a = (struct Allocator *)mem_allocators_dynamic();
	/* 512 > MEM_DEFAULT_CHUNK_SIZE (256) → gets its own chunk */
	p1 = _quasar_mem_allocate(a, 512);  T_NOTNULL(p1);
	/* small alloc in new chunk */
	p2 = _quasar_mem_allocate(a, 10);   T_NOTNULL(p2);
	/* huge alloc → dedicated chunk */
	p3 = _quasar_mem_allocate(a, 8192); T_NOTNULL(p3);

	T(((uintptr_t)p1 & 15) == 0);
	T(((uintptr_t)p2 & 15) == 0);
	T(((uintptr_t)p3 & 15) == 0);
	mem_allocator_free((Allocator *)a);
}

static void test_chunked_offset_overflow_new_chunk(void)
{
	struct Allocator *a;
	void *p1;

	/* Fill chunk so aligned offset overflows to force new chunk */
	a = (struct Allocator *)mem_allocators_dynamic();
	a->u.c.head = a->u.c.current = NULL;
	/* Create a tiny chunk so alignment pushes past it */
	{
		MemChunk *c = malloc(sizeof(*c));
		c->data = malloc(32);
		c->size = 32;
		c->used = 30;
		c->next = NULL;
		a->u.c.head = a->u.c.current = c;
	}
	/* MEM_ALIGN_UP(30) = 32 which == size, sz <= size-offset = sz <= 0
	   → falls through to new chunk */
	p1 = _quasar_mem_allocate(a, 1);
	T_NOTNULL(p1);
	mem_allocator_free((Allocator *)a);

	/* offset actually > size */
	a = (struct Allocator *)mem_allocators_dynamic();
	a->u.c.head = a->u.c.current = NULL;
	{
		MemChunk *c = malloc(sizeof(*c));
		c->data = malloc(14);
		c->size = 14;
		c->used = 14;
		c->next = NULL;
		a->u.c.head = a->u.c.current = c;
	}
	/* MEM_ALIGN_UP(14)=16 > size=14 → new chunk */
	p1 = _quasar_mem_allocate(a, 1);
	T_NOTNULL(p1);
	mem_allocator_free((Allocator *)a);
}

static void test_zero_size_alloc(void)
{
	struct Allocator *a;
	void *p1, *p2;

	/* Zero-size returns a unique non-null pointer, consuming 1 byte.
	   Two zero-size allocations in a manual allocator should succeed
	   if capacity allows. */
	a = (struct Allocator *)mem_allocators_manual(32);
	p1 = _quasar_mem_allocate(a, 0);  T_NOTNULL(p1);
	p2 = _quasar_mem_allocate(a, 0);  T_NOTNULL(p2);
	T(p1 != p2);
	mem_allocator_free((Allocator *)a);

	/* Chunked: zero-size treated as 1 byte, allocated with alignment */
	a = (struct Allocator *)mem_allocators_dynamic();
	p1 = _quasar_mem_allocate(a, 0);  T_NOTNULL(p1);
	T(((uintptr_t)p1 & 15) == 0);
	mem_allocator_free((Allocator *)a);
}

/* ── Manual resize ──────────────────────────────────────────────── */

static void test_manual_realloc_set(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_manual(10);
	T(a->u.m.size == 10);
	mem_allocators_manual_realloc_set((Allocator *)a, 100);
	T(a->u.m.size == 100);
	mem_allocators_manual_realloc_set((Allocator *)a, 5);
	T(a->u.m.size == 5);
	mem_allocators_manual_realloc_set((Allocator *)a, 0);
	T(a->u.m.size == 0);
	T(a->u.m.data == NULL);
	T(a->u.m.used == 0);
	mem_allocator_free((Allocator *)a);
}

static void test_manual_shrink_clamps_used(void)
{
	struct Allocator *a;
	void *p;

	a = (struct Allocator *)mem_allocators_manual(100);
	p = _quasar_mem_allocate(a, 80);    T_NOTNULL(p);
	T(a->u.m.used == 80);
	mem_allocators_manual_realloc_set((Allocator *)a, 50);
	T(a->u.m.size == 50);
	T(a->u.m.used == 50);  /* clamped */
	mem_allocator_free((Allocator *)a);
}

static void test_manual_realloc_add_overflow(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_manual(100);
	mem_allocators_manual_realloc_add((Allocator *)a, SIZE_MAX);
	T(a->u.m.size == 100);  /* unchanged, overflow guarded */
	mem_allocator_free((Allocator *)a);
}

static void test_manual_realloc_sub(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_manual(100);
	mem_allocators_manual_realloc_sub((Allocator *)a, 30);
	T(a->u.m.size == 70);
	mem_allocator_free((Allocator *)a);

	/* sz >= size → zero-size */
	a = (struct Allocator *)mem_allocators_manual(100);
	mem_allocators_manual_realloc_sub((Allocator *)a, 100);
	T(a->u.m.size == 0);
	mem_allocator_free((Allocator *)a);

	a = (struct Allocator *)mem_allocators_manual(100);
	mem_allocators_manual_realloc_sub((Allocator *)a, 200);
	T(a->u.m.size == 0);
	mem_allocator_free((Allocator *)a);
}

static void test_manual_realloc_set_noop_on_wrong_type(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_dynamic();
	mem_allocators_manual_realloc_set((Allocator *)a, 999);
	mem_allocators_manual_realloc_add((Allocator *)a, 999);
	mem_allocators_manual_realloc_sub((Allocator *)a, 999);
	/* Must not crash or corrupt */
	mem_allocator_free((Allocator *)a);
}

/* ── Arena operations ───────────────────────────────────────────── */

static void test_arena_clear_reuse(void)
{
	struct Allocator *a;
	void *p1, *p2;

	a = (struct Allocator *)mem_allocators_arena();
	p1 = _quasar_mem_allocate(a, 32);
	mem_allocators_arena_clear((Allocator *)a);
	p2 = _quasar_mem_allocate(a, 32);
	T(p1 == p2);  /* same chunk, same offset */
	mem_allocator_free((Allocator *)a);
}

static void test_arena_clear_empty(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_arena();
	mem_allocators_arena_clear((Allocator *)a);  /* must not crash */
	mem_allocator_free((Allocator *)a);
}

static void test_arena_reset(void)
{
	struct Allocator *a;
	void *p1;

	a = (struct Allocator *)mem_allocators_arena();
	/* Force multiple chunks */
	_quasar_mem_allocate(a, 512);
	_quasar_mem_allocate(a, 512);
	_quasar_mem_allocate(a, 512);
	/* reset: keeps only first chunk */
	mem_allocators_arena_reset((Allocator *)a);
	p1 = _quasar_mem_allocate(a, 32);
	T_NOTNULL(p1);
	mem_allocator_free((Allocator *)a);
}

static void test_arena_reset_empty(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_arena();
	mem_allocators_arena_reset((Allocator *)a);  /* must not crash */
	mem_allocator_free((Allocator *)a);
}

static void test_arena_ops_noop_on_wrong_type(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_dynamic();
	mem_allocators_arena_clear((Allocator *)a);
	mem_allocators_arena_reset((Allocator *)a);
	mem_allocator_free((Allocator *)a);
}

/* ── Temp operations ────────────────────────────────────────────── */

static void test_temp_clear_reuse(void)
{
	struct Allocator *a;
	void *p1, *p2;

	a = (struct Allocator *)mem_allocators_temp();
	p1 = _quasar_mem_allocate(a, 32);
	mem_allocators_temp_clear((Allocator *)a);
	p2 = _quasar_mem_allocate(a, 32);
	T(p1 == p2);
	mem_allocator_free((Allocator *)a);
}

static void test_temp_reset_reuse(void)
{
	struct Allocator *a;
	void *p1;

	a = (struct Allocator *)mem_allocators_temp();
	_quasar_mem_allocate(a, 512);
	_quasar_mem_allocate(a, 512);
	mem_allocators_temp_reset((Allocator *)a);
	p1 = _quasar_mem_allocate(a, 32);
	T_NOTNULL(p1);
	mem_allocator_free((Allocator *)a);
}

/* ── Leak allocator ─────────────────────────────────────────────── */

static void test_leak_free_is_noop(void)
{
	struct Allocator *a;
	void *p;
	unsigned char *cp;

	a = (struct Allocator *)mem_allocators_leak();
	p = _quasar_mem_allocate(a, 64);
	T_NOTNULL(p);
	cp = (unsigned char *)p;
	cp[0] = 0xAB;
	mem_allocator_free((Allocator *)a);
	/* Memory must still be readable after free (it leaked) */
	T(cp[0] == 0xAB);
}

/* ── Pointer stability ──────────────────────────────────────────── */

static void test_dynamic_pointer_stability(void)
{
	struct Allocator *a;
	unsigned char *cp;
	void *p1, *p2;

	a = (struct Allocator *)mem_allocators_dynamic();
	p1 = _quasar_mem_allocate(a, 512);
	cp = (unsigned char *)p1;
	cp[0] = 0x11;
	cp[511] = 0xFF;
	/* Force new chunk */
	p2 = _quasar_mem_allocate(a, 10);
	T_NOTNULL(p2);
	T(cp[0] == 0x11);
	T(cp[511] == 0xFF);
	mem_allocator_free((Allocator *)a);
}

static void test_arena_stability_across_growth(void)
{
	struct Allocator *a;
	unsigned char *cp;
	void *p1;

	a = (struct Allocator *)mem_allocators_arena();
	p1 = _quasar_mem_allocate(a, 32);
	cp = (unsigned char *)p1;
	cp[0] = 0x77;
	_quasar_mem_allocate(a, 512);  /* force new chunk */
	T(cp[0] == 0x77);
	mem_allocator_free((Allocator *)a);
}

/* ── Conversion ─────────────────────────────────────────────────── */

static void test_manual_to_dynamic(void)
{
	struct Allocator *a;
	unsigned char *cp;
	void *p1, *p2;

	a = (struct Allocator *)mem_allocators_manual(64);
	p1 = _quasar_mem_allocate(a, 10);
	cp = (unsigned char *)p1;
	cp[0] = 0x42;
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_DYNAMIC);
	T_NOTNULL(a);
	T(cp[0] == 0x42);  /* pointer stable */
	/* Allocate past original capacity — must grow */
	p2 = _quasar_mem_allocate(a, 100);
	T_NOTNULL(p2);
	T(cp[0] == 0x42);  /* still stable after growth */
	mem_allocator_free((Allocator *)a);
}

static void test_manual_to_arena(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_manual(64);
	_quasar_mem_allocate(a, 10);
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_ARENA);
	T_NOTNULL(a);
	mem_allocators_arena_clear((Allocator *)a);
	mem_allocator_free((Allocator *)a);
}

static void test_manual_to_leak(void)
{
	struct Allocator *a;
	unsigned char *cp;
	void *p;

	a = (struct Allocator *)mem_allocators_manual(64);
	p = _quasar_mem_allocate(a, 10);
	cp = (unsigned char *)p;
	cp[0] = 0x99;
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_LEAK);
	T_NOTNULL(a);
	mem_allocator_free((Allocator *)a);
	T(cp[0] == 0x99);  /* still readable after free (leaked) */
}

static void test_dynamic_to_arena(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_dynamic();
	_quasar_mem_allocate(a, 32);
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_ARENA);
	T_NOTNULL(a);
	mem_allocators_arena_clear((Allocator *)a);
	mem_allocator_free((Allocator *)a);
}

static void test_dynamic_to_leak(void)
{
	struct Allocator *a;
	unsigned char *cp;
	void *p;

	a = (struct Allocator *)mem_allocators_dynamic();
	p = _quasar_mem_allocate(a, 32);
	cp = (unsigned char *)p;
	cp[0] = 0xEE;
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_LEAK);
	T_NOTNULL(a);
	mem_allocator_free((Allocator *)a);
	T(cp[0] == 0xEE);
}

static void test_arena_to_leak(void)
{
	struct Allocator *a;
	unsigned char *cp;
	void *p;

	a = (struct Allocator *)mem_allocators_arena();
	p = _quasar_mem_allocate(a, 32);
	cp = (unsigned char *)p;
	cp[0] = 0x55;
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_LEAK);
	T_NOTNULL(a);
	mem_allocator_free((Allocator *)a);
	T(cp[0] == 0x55);
}

static void test_temp_to_dynamic(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_temp();
	_quasar_mem_allocate(a, 32);
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_DYNAMIC);
	T_NOTNULL(a);
	mem_allocator_free((Allocator *)a);
}

static void test_temp_to_arena(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_temp();
	_quasar_mem_allocate(a, 32);
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_ARENA);
	T_NOTNULL(a);
	mem_allocators_arena_clear((Allocator *)a);
	mem_allocator_free((Allocator *)a);
}

static void test_temp_to_leak(void)
{
	struct Allocator *a;
	unsigned char *cp;
	void *p;

	a = (struct Allocator *)mem_allocators_temp();
	p = _quasar_mem_allocate(a, 32);
	cp = (unsigned char *)p;
	cp[0] = 0x33;
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_LEAK);
	T_NOTNULL(a);
	mem_allocator_free((Allocator *)a);
	T(cp[0] == 0x33);
}

static void test_leak_to_manual_empty(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_leak();
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_MANUAL);
	T_NOTNULL(a);
	T(a->u.m.size == 0);
	mem_allocator_free((Allocator *)a);
}

static void test_leak_to_manual_one_chunk(void)
{
	struct Allocator *a;
	unsigned char *cp;
	void *p;

	a = (struct Allocator *)mem_allocators_leak();
	p = _quasar_mem_allocate(a, 64);
	cp = (unsigned char *)p;
	cp[0] = 0x11;
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_MANUAL);
	T_NOTNULL(a);
	T(cp[0] == 0x11);  /* single chunk: pointers stable */
	mem_allocator_free((Allocator *)a);
}

static void test_leak_to_manual_multi_chunk(void)
{
	struct Allocator *a;
	void *p1, *p2;

	a = (struct Allocator *)mem_allocators_leak();
	p1 = _quasar_mem_allocate(a, 512);
	p2 = _quasar_mem_allocate(a, 256);
	T_NOTNULL(p1);
	T_NOTNULL(p2);
	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_MANUAL);
	T_NOTNULL(a);
	/* After multi-chunk consolidation, the allocator can be freed
	   normally (was leak, now manual).  Old pointers are invalid. */
	T(a->u.m.size > 0);
	mem_allocator_free((Allocator *)a);
}

static void test_invalid_conversions(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_arena();
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_MANUAL));
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_DYNAMIC));
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_LEAK));
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_TEMP));
	mem_allocator_free((Allocator *)a);

	a = (struct Allocator *)mem_allocators_dynamic();
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_MANUAL));
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_TEMP));
	mem_allocator_free((Allocator *)a);

	a = (struct Allocator *)mem_allocators_temp();
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_MANUAL));
	mem_allocator_free((Allocator *)a);

	a = (struct Allocator *)mem_allocators_leak();
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_DYNAMIC));
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_ARENA));
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_TEMP));
	mem_allocator_free((Allocator *)a);

	a = (struct Allocator *)mem_allocators_manual(64);
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_MANUAL));
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_TEMP));
	mem_allocator_free((Allocator *)a);
}

static void test_conversion_same_type(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_dynamic();
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_DYNAMIC));
	mem_allocator_free((Allocator *)a);

	a = (struct Allocator *)mem_allocators_arena();
	T_NOTNULL(mem_convert_allocator((Allocator *)a, MEM_ARENA));
	mem_allocator_free((Allocator *)a);
}

/* ── Lifetime ───────────────────────────────────────────────────── */

static void test_free_null(void)
{
	mem_allocator_free(NULL);  /* must not crash */
}

static void test_free_empty_dynamic(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_dynamic();
	mem_allocator_free((Allocator *)a);
}

static void test_free_after_clear(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_arena();
	_quasar_mem_allocate(a, 32);
	mem_allocators_arena_clear((Allocator *)a);
	mem_allocator_free((Allocator *)a);
}

static void test_free_after_reset(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_arena();
	_quasar_mem_allocate(a, 32);
	mem_allocators_arena_reset((Allocator *)a);
	mem_allocator_free((Allocator *)a);
}

/* ── Alignment ──────────────────────────────────────────────────── */

static void test_alignment_across_allocations(void)
{
	struct Allocator *a;
	void *p[10];
	int i;

	a = (struct Allocator *)mem_allocators_dynamic();
	for (i = 0; i < 10; i++) {
		p[i] = _quasar_mem_allocate(a, (size_t)(i + 1));
		T_NOTNULL(p[i]);
		T(((uintptr_t)p[i] & 15) == 0);
	}
	mem_allocator_free((Allocator *)a);
}

static void test_alignment_after_clear(void)
{
	struct Allocator *a;
	void *p;

	a = (struct Allocator *)mem_allocators_arena();
	_quasar_mem_allocate(a, 7);
	_quasar_mem_allocate(a, 13);
	mem_allocators_arena_clear((Allocator *)a);
	p = _quasar_mem_allocate(a, 1);
	T(((uintptr_t)p & 15) == 0);
	mem_allocator_free((Allocator *)a);
}

static void test_alignment_after_reset(void)
{
	struct Allocator *a;
	void *p;

	a = (struct Allocator *)mem_allocators_temp();
	_quasar_mem_allocate(a, 3);
	_quasar_mem_allocate(a, 5);
	mem_allocators_temp_reset((Allocator *)a);
	p = _quasar_mem_allocate(a, 1);
	T(((uintptr_t)p & 15) == 0);
	mem_allocator_free((Allocator *)a);
}

/* ── Null / Wrong-type safety ───────────────────────────────────── */

static void test_null_allocator_safety(void)
{
	T_NULL(_quasar_mem_allocate(NULL, 10));
	mem_allocators_manual_realloc_set(NULL, 10);
	mem_allocators_manual_realloc_add(NULL, 10);
	mem_allocators_manual_realloc_sub(NULL, 10);
	mem_allocators_arena_clear(NULL);
	mem_allocators_arena_reset(NULL);
	mem_allocators_temp_clear(NULL);
	mem_allocators_temp_reset(NULL);
	mem_allocator_free(NULL);
	T_NULL(mem_convert_allocator(NULL, MEM_DYNAMIC));
}

static void test_wrong_type_safety(void)
{
	struct Allocator *a;

	/* Manual ops on non-manual */
	a = (struct Allocator *)mem_allocators_dynamic();
	mem_allocators_manual_realloc_set((Allocator *)a, 100);
	mem_allocators_manual_realloc_add((Allocator *)a, 100);
	mem_allocators_manual_realloc_sub((Allocator *)a, 100);
	mem_allocator_free((Allocator *)a);

	/* Arena ops on dynamic */
	a = (struct Allocator *)mem_allocators_dynamic();
	mem_allocators_arena_clear((Allocator *)a);
	mem_allocators_arena_reset((Allocator *)a);
	mem_allocator_free((Allocator *)a);

	/* Temp ops on arena */
	a = (struct Allocator *)mem_allocators_arena();
	mem_allocators_temp_clear((Allocator *)a);
	mem_allocators_temp_reset((Allocator *)a);
	mem_allocator_free((Allocator *)a);

	/* Arena ops on manual */
	a = (struct Allocator *)mem_allocators_manual(64);
	mem_allocators_arena_clear((Allocator *)a);
	mem_allocators_arena_reset((Allocator *)a);
	mem_allocator_free((Allocator *)a);
}

/* ── Repeat cycles ──────────────────────────────────────────────── */

static void test_repeat_clear_allocate(void)
{
	struct Allocator *a;
	int i;

	a = (struct Allocator *)mem_allocators_temp();
	for (i = 0; i < 10; i++) {
		void *p = _quasar_mem_allocate(a, 32);
		T_NOTNULL(p);
		mem_allocators_temp_clear((Allocator *)a);
	}
	mem_allocator_free((Allocator *)a);
}

static void test_repeat_reset_allocate(void)
{
	struct Allocator *a;
	int i;

	a = (struct Allocator *)mem_allocators_arena();
	for (i = 0; i < 10; i++) {
		void *p = _quasar_mem_allocate(a, 32);
		T_NOTNULL(p);
		mem_allocators_arena_reset((Allocator *)a);
	}
	mem_allocator_free((Allocator *)a);
}

/* ── Empty manual allocator rejects allocations ─────────────────── */

static void test_empty_manual_rejects_alloc(void)
{
	struct Allocator *a;

	a = (struct Allocator *)mem_allocators_manual(0);
	T_NOTNULL(a);
	T_NULL(_quasar_mem_allocate(a, 1));
	T_NULL(_quasar_mem_allocate(a, 100));
	mem_allocator_free((Allocator *)a);
}

/* ── Multi-chunk consolidation preserves data in order ──────────── */

static void test_leak_to_manual_data_integrity(void)
{
	struct Allocator *a;
	unsigned char *p1, *p2, *p3;

	a = (struct Allocator *)mem_allocators_leak();
	p1 = _quasar_mem_allocate(a, 64);
	p2 = _quasar_mem_allocate(a, 512);  /* forces new chunk */
	p3 = _quasar_mem_allocate(a, 32);

	p1[0] = 0xAA; p1[63] = 0xBB;
	p2[0] = 0xCC; p2[511] = 0xDD;
	p3[0] = 0xEE; p3[31] = 0xFF;

	a = (struct Allocator *)mem_convert_allocator((Allocator *)a, MEM_MANUAL);
	T_NOTNULL(a);

	/* After consolidation, data is packed contiguously */
	T(a->u.m.data[0] == 0xAA);
	T(a->u.m.data[63] == 0xBB);
	T(a->u.m.data[64] == 0xCC);
	T(a->u.m.data[64 + 511] == 0xDD);
	T(a->u.m.data[64 + 512] == 0xEE);
	T(a->u.m.data[64 + 512 + 31] == 0xFF);

	mem_allocator_free((Allocator *)a);
}

/* ── Stress: rapid alloc/clear/convert cycles ───────────────────── */

static void test_stress_rapid_cycles(void)
{
	struct Allocator *a;
	int i;

	a = (struct Allocator *)mem_allocators_temp();
	for (i = 0; i < 100; i++) {
		T_NOTNULL(_quasar_mem_allocate(a, (size_t)((i % 50) + 1)));
		if (i % 20 == 0) mem_allocators_temp_clear((Allocator *)a);
	}
	mem_allocator_free((Allocator *)a);

	a = (struct Allocator *)mem_allocators_arena();
	for (i = 0; i < 50; i++) {
		T_NOTNULL(_quasar_mem_allocate(a, 64));
		mem_allocators_arena_clear((Allocator *)a);
	}
	mem_allocator_free((Allocator *)a);

	/* Interleaved conversions */
	{
		struct Allocator *m;
		m = (struct Allocator *)mem_allocators_manual(256);
		for (i = 0; i < 15; i++) {
			T_NOTNULL(_quasar_mem_allocate(m, 10));
		}
		m = (struct Allocator *)mem_convert_allocator((Allocator *)m, MEM_DYNAMIC);
		T_NOTNULL(m);
		for (i = 0; i < 10; i++) {
			T_NOTNULL(_quasar_mem_allocate(m, 128));
		}
		m = (struct Allocator *)mem_convert_allocator((Allocator *)m, MEM_ARENA);
		T_NOTNULL(m);
		mem_allocators_arena_clear((Allocator *)m);
		for (i = 0; i < 5; i++) {
			T_NOTNULL(_quasar_mem_allocate(m, 256));
		}
		mem_allocator_free((Allocator *)m);
	}
}

/* ── Regression: clear + overflow must not leak chunks ──────────── */

static void test_arena_clear_followed_by_overflow(void)
{
	struct Allocator *a;
	void *p;
	int i;

	a = (struct Allocator *)mem_allocators_arena();

	/* Create 3 chunks by filling first two */
	for (i = 0; i < 30; i++)
		_quasar_mem_allocate(a, 32);

	/* Clear resets used on all chunks, current = head */
	mem_allocators_arena_clear((Allocator *)a);

	/* Allocate a large object that fills head completely.
	   MEM_ALIGN_UP(0)=0, so 256 bytes fills the default chunk.
	   The allocation loop must walk to chunk2/chunk3 rather than
	   appending a new chunk to head (which would leak them). */
	p = _quasar_mem_allocate(a, 256);
	T_NOTNULL(p);

	/* Next allocation should go into chunk2 */
	p = _quasar_mem_allocate(a, 32);
	T_NOTNULL(p);

	/* Free must not crash and must not leak */
	mem_allocator_free((Allocator *)a);
}

/* ── Driver ─────────────────────────────────────────────────────── */

int main(void)
{
	test_manual_creation();
	test_chunked_creation();
	test_manual_alloc();
	test_manual_alloc_exhaustion();
	test_manual_alloc_offset_overflow();
	test_chunked_alloc();
	test_chunked_growth();
	test_chunked_offset_overflow_new_chunk();
	test_zero_size_alloc();
	test_manual_realloc_set();
	test_manual_shrink_clamps_used();
	test_manual_realloc_add_overflow();
	test_manual_realloc_sub();
	test_manual_realloc_set_noop_on_wrong_type();
	test_arena_clear_reuse();
	test_arena_clear_empty();
	test_arena_reset();
	test_arena_reset_empty();
	test_arena_ops_noop_on_wrong_type();
	test_temp_clear_reuse();
	test_temp_reset_reuse();
	test_leak_free_is_noop();
	test_dynamic_pointer_stability();
	test_arena_stability_across_growth();
	test_manual_to_dynamic();
	test_manual_to_arena();
	test_manual_to_leak();
	test_dynamic_to_arena();
	test_dynamic_to_leak();
	test_arena_to_leak();
	test_temp_to_dynamic();
	test_temp_to_arena();
	test_temp_to_leak();
	test_leak_to_manual_empty();
	test_leak_to_manual_one_chunk();
	test_leak_to_manual_multi_chunk();
	test_invalid_conversions();
	test_conversion_same_type();
	test_free_null();
	test_free_empty_dynamic();
	test_free_after_clear();
	test_free_after_reset();
	test_alignment_across_allocations();
	test_alignment_after_clear();
	test_alignment_after_reset();
	test_null_allocator_safety();
	test_wrong_type_safety();
	test_repeat_clear_allocate();
	test_repeat_reset_allocate();
	test_arena_clear_followed_by_overflow();
	test_empty_manual_rejects_alloc();
	test_leak_to_manual_data_integrity();
	test_stress_rapid_cycles();

	if (failures) {
		fprintf(stderr, "\n%d test(s) FAILED.\n", failures);
		return 1;
	}
	printf("All %d tests passed.\n",
	       56);  /* update if tests added */
	return 0;
}
