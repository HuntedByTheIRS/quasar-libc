#ifndef QUASAR_CORE_DATAPRIM_H
#define QUASAR_CORE_DATAPRIM_H

#include <stdbool.h>
#include <stddef.h>

#include <quasar/core/str.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Forward declaration: the full Allocator struct is defined in
 * src/quasar/core/mem.h.  Public headers use the opaque typedef
 * from <quasar/std/memory.h>.  core/dataprim.h only needs the
 * pointer type for field declarations.
 */
typedef struct Allocator Allocator;

/*
 * ── Dynamic Array ──────────────────────────────────────────────────
 *
 * array_t is a contiguous, resizable container for elements of a
 * fixed size.  The array owns its storage through the allocator
 * used to create it.  All operations are O(1) except where noted.
 *
 * A zeroed array_t ({NULL, NULL, 0, 0, 0}) represents an invalid /
 * allocation-failed array.
 */
typedef struct {
	Allocator     *allocator;
	unsigned char *data;
	size_t         len;
	size_t         capacity;
	size_t         element_size;
} array_t;

/*
 * ── Hash Map ───────────────────────────────────────────────────────
 *
 * map_t is an open-addressing hash map with str_t keys and str_t
 * values.  Linear probing is used for collision resolution.
 *
 * A zeroed map_t ({NULL, NULL, 0, 0}) represents an invalid /
 * allocation-failed map.
 */

typedef struct {
	str_t  key;
	str_t  value;
	bool   occupied;	/* false = empty slot (never used or tombstone) */
} MapEntry;

typedef struct {
	Allocator *allocator;
	MapEntry  *entries;
	size_t     len;		/* number of live (non-tombstone) entries */
	size_t     capacity;	/* total slot count (always a power of two) */
} map_t;

/*
 * ── Singly Linked List ─────────────────────────────────────────────
 *
 * list_t is a singly linked list of opaque elements.  Each node
 * stores a copy of the user's data (memcpy'd from the provided
 * pointer).  The list owns its nodes through the allocator.
 *
 * A zeroed list_t represents an invalid / allocation-failed list.
 */

typedef struct ListNode {
	void           *data;
	size_t          data_size;
	struct ListNode *next;
} ListNode;

typedef struct {
	Allocator *allocator;
	ListNode  *head;
	ListNode  *tail;
	size_t     len;
	size_t     element_size;
} list_t;

/*
 * ── Doubly Linked List ─────────────────────────────────────────────
 *
 * dlist_t is a doubly linked list of opaque elements.  Each node
 * stores a copy of the user's data.  Supports O(1) insertion and
 * removal at both ends, and O(1) insertion before/after a known
 * node.
 *
 * A zeroed dlist_t represents an invalid / allocation-failed list.
 */

typedef struct DListNode {
	void            *data;
	size_t           data_size;
	struct DListNode *prev;
	struct DListNode *next;
} DListNode;

typedef struct {
	Allocator  *allocator;
	DListNode  *head;
	DListNode  *tail;
	size_t      len;
	size_t      element_size;
} dlist_t;

/*
 * ── Ring Buffer ────────────────────────────────────────────────────
 *
 * ring_t is a circular buffer of fixed-capacity (or dynamically
 * growing) elements.  Push and pop are O(1).  Elements are never
 * shifted; indices wrap around the internal buffer.
 *
 * A zeroed ring_t represents an invalid / allocation-failed ring.
 */

typedef struct {
	Allocator     *allocator;
	unsigned char *data;
	size_t         head;		/* read position (index of oldest element) */
	size_t         tail;		/* write position (index where next push lands) */
	size_t         len;		/* number of elements currently stored */
	size_t         capacity;	/* max element count */
	size_t         element_size;
	bool           dynamic;	/* grows when full instead of rejecting push */
} ring_t;

#ifdef __cplusplus
}
#endif

#endif
