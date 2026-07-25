#include <string.h>

#include "../core/mem.h"
#include <quasar/std/datatypes.h>

/*
 * FNV-1a 64-bit constants used by both map_hash() and the public
 * hash_*() family.
 */
#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME  1099511628211ULL

#define NO_TOMBSTONE SIZE_MAX

/*
 * Hash len bytes of arbitrary data with FNV-1a.  Returns a size_t
 * suitable for use as a hash-table index.
 */
static size_t
fnv_hash(const void *data, size_t len)
{
	const unsigned char *bytes;
	size_t               hash;
	size_t               i;

	bytes = (const unsigned char *)data;
	hash  = FNV_OFFSET;
	for (i = 0; i < len; i++) {
		hash ^= bytes[i];
		hash *= FNV_PRIME;
	}
	return hash;
}

/* Round n up to the next power of two.  Returns at least 1. */
static size_t
next_pow2(size_t n)
{
	if (n == 0) return 1;
	if (n >= (size_t)1 << (sizeof(size_t) * 8 - 1)) return 0; /* would overflow */
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	return n + 1;
}

/* Return true if a * b would overflow size_t. */
static inline bool
mul_would_overflow(size_t a, size_t b)
{
	return b != 0 && a > SIZE_MAX / b;
}

/* ================================================================== */
/*  Dynamic Array                                                      */
/* ================================================================== */

/*
 * Grow the array's backing storage to at least min_capacity elements.
 * Existing data is copied to the new buffer.  On overflow or
 * allocation failure the array is unchanged and false is returned.
 */
static bool
array_grow(array_t *a, size_t min_capacity)
{
	unsigned char *new_data;
	size_t         new_cap;

	if (!a || !a->allocator) return false;

	new_cap = a->capacity;
	if (new_cap < 1) new_cap = 1;

	while (new_cap < min_capacity) {
		if (new_cap > SIZE_MAX / 2) return false;
		new_cap *= 2;
	}

	if (mul_would_overflow(new_cap, a->element_size)) return false;

	new_data = _quasar_mem_allocate(a->allocator,
	                                new_cap * a->element_size);
	if (!new_data) return false;

	if (a->data && a->len > 0)
		memcpy(new_data, a->data, a->len * a->element_size);

	a->data     = new_data;
	a->capacity = new_cap;
	return true;
}

/* ------------------------------------------------------------------ */
/*  Creation                                                           */
/* ------------------------------------------------------------------ */

array_t
array_new(Allocator *a, size_t element_size)
{
	return array_with_capacity(a, element_size, 8);
}

array_t
array_with_capacity(Allocator *a, size_t element_size, size_t capacity)
{
	array_t result = {0};

	if (!a) return result;
	if (capacity == 0) capacity = 8;

	if (mul_would_overflow(capacity, element_size)) return result;

	result.data = _quasar_mem_allocate(a, capacity * element_size);
	if (!result.data) return result;

	result.allocator    = a;
	result.len          = 0;
	result.capacity     = capacity;
	result.element_size = element_size;
	return result;
}

/* ------------------------------------------------------------------ */
/*  Destruction                                                        */
/* ------------------------------------------------------------------ */

void
array_free(array_t *array)
{
	if (!array) return;
	*array = (array_t){0};
}

/* ------------------------------------------------------------------ */
/*  Inspection                                                         */
/* ------------------------------------------------------------------ */

size_t
array_len(array_t array)
{
	return array.len;
}

size_t
array_capacity(array_t array)
{
	return array.capacity;
}

size_t
array_element_size(array_t array)
{
	return array.element_size;
}

/* ------------------------------------------------------------------ */
/*  Element Access                                                     */
/* ------------------------------------------------------------------ */

void *
array_at(array_t array, size_t index)
{
	return array.data + index * array.element_size;
}

const void *
array_at_const(array_t array, size_t index)
{
	return array.data + index * array.element_size;
}

/* ------------------------------------------------------------------ */
/*  Mutation                                                           */
/* ------------------------------------------------------------------ */

void
array_push(array_t *array, const void *element)
{
	if (!array || !element || !array->allocator) return;

	if (array->len >= array->capacity) {
		if (!array_grow(array, array->len + 1)) return;
	}

	memcpy(array->data + array->len * array->element_size,
	       element, array->element_size);
	array->len++;
}

void
array_pop(array_t *array, void *out)
{
	if (!array || !out || array->len == 0 || !array->allocator) return;

	array->len--;
	memcpy(out,
	       array->data + array->len * array->element_size,
	       array->element_size);
}

void
array_insert(array_t *array, size_t index, const void *element)
{
	if (!array || !element || !array->allocator) return;
	if (index > array->len) return;

	if (array->len >= array->capacity) {
		if (!array_grow(array, array->len + 1)) return;
	}

	if (index < array->len) {
		memmove(array->data + (index + 1) * array->element_size,
		        array->data + index * array->element_size,
		        (array->len - index) * array->element_size);
	}

	memcpy(array->data + index * array->element_size,
	       element, array->element_size);
	array->len++;
}

void
array_remove(array_t *array, size_t index, void *out)
{
	if (!array || !out || index >= array->len) return;

	memcpy(out,
	       array->data + index * array->element_size,
	       array->element_size);

	if (index + 1 < array->len) {
		memmove(array->data + index * array->element_size,
		        array->data + (index + 1) * array->element_size,
		        (array->len - index - 1) * array->element_size);
	}

	array->len--;
}

void
array_clear(array_t *array)
{
	if (!array) return;
	array->len = 0;
}

/* ------------------------------------------------------------------ */
/*  Capacity Management                                                */
/* ------------------------------------------------------------------ */

bool
array_reserve(array_t *array, size_t capacity)
{
	if (!array) return false;
	if (array->capacity >= capacity) return true;
	return array_grow(array, capacity);
}

bool
array_resize(array_t *array, size_t length)
{
	if (!array) return false;

	if (length > array->capacity) {
		if (!array_grow(array, length)) return false;
	}

	if (length > array->len) {
		memset(array->data + array->len * array->element_size, 0,
		       (length - array->len) * array->element_size);
	}

	array->len = length;
	return true;
}

bool
array_shrink_to_fit(array_t *array)
{
	unsigned char *new_data;

	if (!array) return false;
	if (array->len == array->capacity) return true;

	if (array->len == 0) {
		/*
		 * With arena/bump allocators the old buffer cannot be
		 * freed individually; it persists in the arena until
		 * the allocator is destroyed.  Setting data to NULL
		 * and capacity to 0 fulfils the shrink-to-fit contract
		 * (len == capacity == 0).  The caller sees a correctly
		 * zeroed array that will grow on next push.
		 */
		array->data     = NULL;
		array->capacity = 0;
		return true;
	}

	if (mul_would_overflow(array->len, array->element_size)) return false;

	{
		size_t new_size = array->len * array->element_size;

		new_data = _quasar_mem_allocate(array->allocator, new_size);
		if (!new_data) return false;

		memcpy(new_data, array->data, new_size);
	}
	array->data     = new_data;
	array->capacity = array->len;
	return true;
}

/* ================================================================== */
/*  Hash Map                                                           */
/* ================================================================== */

static size_t
map_hash(str_t key)
{
	return fnv_hash(key.data, key.len);
}

/*
 * Allocate a new entry table of new_capacity slots, rehash all live
 * entries from the old table, and swap it in.  Tombstones are dropped.
 * Returns true on success; on failure the map is unchanged.
 */
static bool
	map_grow(map_t *m, size_t new_capacity)
{
	MapEntry *old_entries;
	MapEntry *new_entries;
	size_t    old_capacity;
	size_t    i;

	old_entries  = m->entries;
	old_capacity = m->capacity;

	if (!old_entries && old_capacity > 0) return false;

	if (mul_would_overflow(new_capacity, sizeof(MapEntry))) return false;

	new_entries = _quasar_mem_allocate(m->allocator,
	                                   new_capacity * sizeof(MapEntry));
	if (!new_entries) return false;
	memset(new_entries, 0, new_capacity * sizeof(MapEntry));

	m->entries  = new_entries;
	m->capacity = new_capacity;
	m->len      = 0;

	for (i = 0; i < old_capacity; i++) {
		MapEntry *e = &old_entries[i];
		size_t    hash;
		size_t    slot;

		if (!e->occupied || !e->key.data) continue;

		hash = map_hash(e->key);
		slot = hash & (new_capacity - 1);

		while (new_entries[slot].occupied)
			slot = (slot + 1) & (new_capacity - 1);

		new_entries[slot] = *e;
		m->len++;
	}

	return true;
}

/* ------------------------------------------------------------------ */
/*  Creation                                                           */
/* ------------------------------------------------------------------ */

map_t
map_new(Allocator *a)
{
	return map_new_with_capacity(a, 8);
}

map_t
map_new_with_capacity(Allocator *a, size_t capacity)
{
	map_t    result = {0};
	size_t   cap;
	MapEntry *entries;

	if (!a) return result;

	cap = next_pow2(capacity);
	if (cap < 8) cap = 8;

	if (mul_would_overflow(cap, sizeof(MapEntry))) return result;

	entries = _quasar_mem_allocate(a, cap * sizeof(MapEntry));
	if (!entries) return result;
	memset(entries, 0, cap * sizeof(MapEntry));

	result.allocator = a;
	result.entries   = entries;
	result.len       = 0;
	result.capacity  = cap;
	return result;
}

/* ------------------------------------------------------------------ */
/*  Destruction                                                        */
/* ------------------------------------------------------------------ */

void
map_free(map_t *map)
{
	if (!map) return;
	*map = (map_t){0};
}

/* ------------------------------------------------------------------ */
/*  Inspection                                                         */
/* ------------------------------------------------------------------ */

size_t
map_len(map_t map)
{
	return map.len;
}

size_t
map_capacity(map_t map)
{
	return map.capacity;
}

/* ------------------------------------------------------------------ */
/*  Operations                                                         */
/* ------------------------------------------------------------------ */

bool
map_insert(map_t *map, str_t key, str_t value)
{
	size_t hash;
	size_t idx;
	size_t first_tombstone;
	size_t i;

	if (!map || !key.data) return false;

	/* Grow if load factor would exceed 0.75 after this insertion. */
	if (map->len >= map->capacity - (map->capacity >> 2)) {
		size_t new_cap;

		new_cap = map->capacity;
		if (new_cap < 1) new_cap = 8;
		if (new_cap > SIZE_MAX / 2) return false;
		new_cap *= 2;
		if (!map_grow(map, new_cap)) return false;
	}

	hash            = map_hash(key);
	idx             = hash & (map->capacity - 1);
	first_tombstone = NO_TOMBSTONE;

	for (i = 0; i < map->capacity; i++) {
		if (map->entries[idx].occupied) {
			/* Occupied slot — check for matching key. */
			if (map->entries[idx].key.len == key.len &&
			    memcmp(map->entries[idx].key.data,
			           key.data, key.len) == 0) {
				/* Key exists, replace value. */
				map->entries[idx].value = value;
				return true;
			}
		} else if (map->entries[idx].key.data) {
			/* Tombstone — remember first one. */
			if (first_tombstone == NO_TOMBSTONE)
				first_tombstone = idx;
		} else {
			size_t target;

			/* Never-used empty slot — insert here
			   (or at the first tombstone if we saw one). */
			target = (first_tombstone != NO_TOMBSTONE)
			             ? first_tombstone
			             : idx;

			map->entries[target].key      = key;
			map->entries[target].value    = value;
			map->entries[target].occupied = true;
			map->len++;
			return true;
		}

		idx = (idx + 1) & (map->capacity - 1);
	}

	/*
	 * Table is fully occupied or all-tombstones (shouldn't happen
	 * while load factor is enforced, but handle it anyway).
	 */
	if (first_tombstone != NO_TOMBSTONE) {
		map->entries[first_tombstone].key      = key;
		map->entries[first_tombstone].value    = value;
		map->entries[first_tombstone].occupied = true;
		map->len++;
		return true;
	}

	/* Truly full — grow and retry. */
	{
		size_t new_cap;

		new_cap = map->capacity * 2;
		if (new_cap == 0) new_cap = 8;
		if (!map_grow(map, new_cap)) return false;
		return map_insert(map, key, value);
	}
}

str_t
map_get(map_t map, str_t key)
{
	size_t hash;
	size_t idx;
	size_t i;

	if (!key.data || map.capacity == 0) {
		str_t zero = {0, 0};
		return zero;
	}

	hash = map_hash(key);
	idx  = hash & (map.capacity - 1);

	for (i = 0; i < map.capacity; i++) {
		if (map.entries[idx].occupied) {
			if (map.entries[idx].key.len == key.len &&
			    memcmp(map.entries[idx].key.data,
			           key.data, key.len) == 0)
				return map.entries[idx].value;
		} else if (!map.entries[idx].key.data) {
			/* Reached a never-used slot — key not present. */
			break;
		}
		/* Tombstone: keep probing. */
		idx = (idx + 1) & (map.capacity - 1);
	}

	{
		str_t zero = {0, 0};
		return zero;
	}
}

bool
map_contains(map_t map, str_t key)
{
	size_t hash;
	size_t idx;
	size_t i;

	if (!key.data || map.capacity == 0) return false;

	hash = map_hash(key);
	idx  = hash & (map.capacity - 1);

	for (i = 0; i < map.capacity; i++) {
		if (map.entries[idx].occupied) {
			if (map.entries[idx].key.len == key.len &&
			    memcmp(map.entries[idx].key.data,
			           key.data, key.len) == 0)
				return true;
		} else if (!map.entries[idx].key.data) {
			break;
		}
		idx = (idx + 1) & (map.capacity - 1);
	}

	return false;
}

void
map_remove(map_t *map, str_t key)
{
	size_t hash;
	size_t idx;
	size_t i;

	if (!map || !key.data || map->capacity == 0) return;

	hash = map_hash(key);
	idx  = hash & (map->capacity - 1);

	for (i = 0; i < map->capacity; i++) {
		if (map->entries[idx].occupied) {
			if (map->entries[idx].key.len == key.len &&
			    memcmp(map->entries[idx].key.data,
			           key.data, key.len) == 0) {
				/*
				 * Convert to tombstone: mark unoccupied
				 * but leave key.data in place so the
				 * probe chain stays intact.
				 */
				map->entries[idx].occupied = false;
				map->len--;
				return;
			}
		} else if (!map->entries[idx].key.data) {
			break;
		}
		idx = (idx + 1) & (map->capacity - 1);
	}
}

void
map_clear(map_t *map)
{
	if (!map || !map->entries) return;
	memset(map->entries, 0, map->capacity * sizeof(MapEntry));
	map->len = 0;
}

bool
map_reserve(map_t *map, size_t capacity)
{
	if (!map) return false;
	if (map->capacity >= capacity) return true;
	return map_grow(map, next_pow2(capacity));
}

/* ================================================================== */
/*  Hash Functions                                                     */
/* ================================================================== */

size_t
hash_i8(int8_t v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_i16(int16_t v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_i32(int32_t v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_i64(int64_t v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_u8(uint8_t v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_u16(uint16_t v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_u32(uint32_t v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_u64(uint64_t v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_f32(float v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_f64(double v)
{
	return fnv_hash(&v, sizeof(v));
}

size_t
hash_str(str_t s)
{
	return fnv_hash(s.data, s.len);
}

size_t
hash_bytes(const void *data, size_t len)
{
	return fnv_hash(data, len);
}

/* ================================================================== */
/*  Singly Linked List                                                 */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/*  Creation                                                           */
/* ------------------------------------------------------------------ */

list_t
list_new(Allocator *a, size_t element_size)
{
	list_t result = {0};

	if (!a) return result;
	result.allocator    = a;
	result.element_size = element_size;
	return result;
}

/* ------------------------------------------------------------------ */
/*  Destruction                                                        */
/* ------------------------------------------------------------------ */

void
list_free(list_t *list)
{
	if (!list) return;
	*list = (list_t){0};
}

/* ------------------------------------------------------------------ */
/*  Inspection                                                         */
/* ------------------------------------------------------------------ */

size_t
list_len(list_t list)
{
	return list.len;
}

/* ------------------------------------------------------------------ */
/*  Mutation                                                           */
/* ------------------------------------------------------------------ */

void
list_push_front(list_t *list, const void *element)
{
	ListNode *node;
	void     *data;

	if (!list || !element) return;

	data = _quasar_mem_allocate(list->allocator, list->element_size);
	if (!data) return;
	node = _quasar_mem_allocate(list->allocator, sizeof(ListNode));
	if (!node) return;

	memcpy(data, element, list->element_size);
	node->data      = data;
	node->data_size = list->element_size;
	node->next      = list->head;
	list->head      = node;

	if (!list->tail) list->tail = node;
	list->len++;
}

void
list_push_back(list_t *list, const void *element)
{
	ListNode *node;
	void     *data;

	if (!list || !element) return;

	data = _quasar_mem_allocate(list->allocator, list->element_size);
	if (!data) return;
	node = _quasar_mem_allocate(list->allocator, sizeof(ListNode));
	if (!node) return;

	memcpy(data, element, list->element_size);
	node->data      = data;
	node->data_size = list->element_size;
	node->next      = NULL;

	if (list->tail) {
		list->tail->next = node;
	} else {
		list->head = node;
	}
	list->tail = node;
	list->len++;
}

void
list_pop_front(list_t *list, void *out)
{
	ListNode *node;

	if (!list || !out || !list->head) return;

	node = list->head;
	memcpy(out, node->data, list->element_size);
	list->head = node->next;

	if (!list->head) list->tail = NULL;
	list->len--;
}

void
list_pop_back(list_t *list, void *out)
{
	ListNode *prev;

	if (!list || !out || !list->head) return;

	if (list->head == list->tail) {
		/* Only one node. */
		memcpy(out, list->head->data, list->element_size);
		list->head = NULL;
		list->tail = NULL;
		list->len--;
		return;
	}

	/* Walk to the second-to-last node (O(n)). */
	prev = list->head;
	while (prev->next != list->tail)
		prev = prev->next;

	memcpy(out, list->tail->data, list->element_size);
	prev->next = NULL;
	list->tail = prev;
	list->len--;
}

void
list_clear(list_t *list)
{
	if (!list) return;
	list->head = NULL;
	list->tail = NULL;
	list->len  = 0;
}

/* ------------------------------------------------------------------ */
/*  Node Access                                                        */
/* ------------------------------------------------------------------ */

ListNode *
list_head(list_t list)
{
	return list.head;
}

ListNode *
list_tail(list_t list)
{
	return list.tail;
}

void *
list_node_data(ListNode *node)
{
	return node ? node->data : NULL;
}

ListNode *
list_node_next(ListNode *node)
{
	return node ? node->next : NULL;
}

/* ================================================================== */
/*  Doubly Linked List                                                 */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/*  Creation                                                           */
/* ------------------------------------------------------------------ */

dlist_t
dlist_new(Allocator *a, size_t element_size)
{
	dlist_t result = {0};

	if (!a) return result;
	result.allocator    = a;
	result.element_size = element_size;
	return result;
}

/* ------------------------------------------------------------------ */
/*  Destruction                                                        */
/* ------------------------------------------------------------------ */

void
dlist_free(dlist_t *list)
{
	if (!list) return;
	*list = (dlist_t){0};
}

/* ------------------------------------------------------------------ */
/*  Inspection                                                         */
/* ------------------------------------------------------------------ */

size_t
dlist_len(dlist_t list)
{
	return list.len;
}

/* ------------------------------------------------------------------ */
/*  Mutation                                                           */
/* ------------------------------------------------------------------ */

void
dlist_push_front(dlist_t *list, const void *element)
{
	DListNode *node;
	void      *data;

	if (!list || !element) return;

	data = _quasar_mem_allocate(list->allocator, list->element_size);
	if (!data) return;
	node = _quasar_mem_allocate(list->allocator, sizeof(DListNode));
	if (!node) return;

	memcpy(data, element, list->element_size);
	node->data      = data;
	node->data_size = list->element_size;
	node->prev      = NULL;
	node->next      = list->head;

	if (list->head) list->head->prev = node;
	list->head = node;
	if (!list->tail) list->tail = node;
	list->len++;
}

void
dlist_push_back(dlist_t *list, const void *element)
{
	DListNode *node;
	void      *data;

	if (!list || !element) return;

	data = _quasar_mem_allocate(list->allocator, list->element_size);
	if (!data) return;
	node = _quasar_mem_allocate(list->allocator, sizeof(DListNode));
	if (!node) return;

	memcpy(data, element, list->element_size);
	node->data      = data;
	node->data_size = list->element_size;
	node->prev      = list->tail;
	node->next      = NULL;

	if (list->tail) list->tail->next = node;
	else list->head = node;
	list->tail = node;
	list->len++;
}

void
dlist_pop_front(dlist_t *list, void *out)
{
	DListNode *node;

	if (!list || !out || !list->head) return;

	node = list->head;
	memcpy(out, node->data, list->element_size);
	list->head = node->next;

	if (list->head) list->head->prev = NULL;
	else list->tail = NULL;
	list->len--;
}

void
dlist_pop_back(dlist_t *list, void *out)
{
	DListNode *node;

	if (!list || !out || !list->tail) return;

	node = list->tail;
	memcpy(out, node->data, list->element_size);
	list->tail = node->prev;

	if (list->tail) list->tail->next = NULL;
	else list->head = NULL;
	list->len--;
}

void
dlist_insert_before(dlist_t *list, DListNode *node, const void *element)
{
	DListNode *new_node;
	void      *data;

	if (!list || !node || !element) return;

	data = _quasar_mem_allocate(list->allocator, list->element_size);
	if (!data) return;
	new_node = _quasar_mem_allocate(list->allocator, sizeof(DListNode));
	if (!new_node) return;

	memcpy(data, element, list->element_size);
	new_node->data      = data;
	new_node->data_size = list->element_size;
	new_node->prev      = node->prev;
	new_node->next      = node;

	if (node->prev) node->prev->next = new_node;
	else list->head = new_node;
	node->prev = new_node;
	list->len++;
}

void
dlist_insert_after(dlist_t *list, DListNode *node, const void *element)
{
	DListNode *new_node;
	void      *data;

	if (!list || !node || !element) return;

	data = _quasar_mem_allocate(list->allocator, list->element_size);
	if (!data) return;
	new_node = _quasar_mem_allocate(list->allocator, sizeof(DListNode));
	if (!new_node) return;

	memcpy(data, element, list->element_size);
	new_node->data      = data;
	new_node->data_size = list->element_size;
	new_node->prev      = node;
	new_node->next      = node->next;

	if (node->next) node->next->prev = new_node;
	else list->tail = new_node;
	node->next = new_node;
	list->len++;
}

void
dlist_remove(dlist_t *list, DListNode *node, void *out)
{
	if (!list || !node) return;

	if (out) memcpy(out, node->data, list->element_size);

	if (node->prev) node->prev->next = node->next;
	else list->head = node->next;

	if (node->next) node->next->prev = node->prev;
	else list->tail = node->prev;

	list->len--;
}

void
dlist_clear(dlist_t *list)
{
	if (!list) return;
	list->head = NULL;
	list->tail = NULL;
	list->len  = 0;
}

/* ------------------------------------------------------------------ */
/*  Node Access                                                        */
/* ------------------------------------------------------------------ */

DListNode *
dlist_head(dlist_t list)
{
	return list.head;
}

DListNode *
dlist_tail(dlist_t list)
{
	return list.tail;
}

void *
dlist_node_data(DListNode *node)
{
	return node ? node->data : NULL;
}

DListNode *
dlist_node_next(DListNode *node)
{
	return node ? node->next : NULL;
}

DListNode *
dlist_node_prev(DListNode *node)
{
	return node ? node->prev : NULL;
}

/* ================================================================== */
/*  Ring Buffer                                                        */
/* ================================================================== */

/*
 * Grow a dynamic ring buffer to double capacity, reordering elements
 * so that head lands at index 0.  Returns false on failure.
 */
static bool
ring_grow(ring_t *ring)
{
	unsigned char *new_data;
	size_t         new_cap;
	size_t         i;

	new_cap = ring->capacity;
	if (new_cap < 1) new_cap = 8;
	else if (new_cap > SIZE_MAX / 2) return false;
	else new_cap *= 2;

	if (mul_would_overflow(new_cap, ring->element_size)) return false;

	new_data = _quasar_mem_allocate(ring->allocator,
	                                new_cap * ring->element_size);
	if (!new_data) return false;

	for (i = 0; i < ring->len; i++) {
		size_t src = (ring->head + i) % ring->capacity;

		memcpy(new_data + i * ring->element_size,
		       ring->data + src * ring->element_size,
		       ring->element_size);
	}

	ring->data     = new_data;
	ring->head     = 0;
	ring->tail     = ring->len;
	ring->capacity = new_cap;
	return true;
}

/* ------------------------------------------------------------------ */
/*  Creation                                                           */
/* ------------------------------------------------------------------ */

ring_t
ring_new(Allocator *a, size_t element_size, size_t capacity)
{
	ring_t result = {0};

	if (!a || capacity == 0) return result;

	if (mul_would_overflow(capacity, element_size)) return result;

	result.data = _quasar_mem_allocate(a, capacity * element_size);
	if (!result.data) return result;

	result.allocator    = a;
	result.head         = 0;
	result.tail         = 0;
	result.len          = 0;
	result.capacity     = capacity;
	result.element_size = element_size;
	result.dynamic      = false;
	return result;
}

ring_t
ring_new_dynamic(Allocator *a, size_t element_size)
{
	ring_t result;

	result = ring_new(a, element_size, 8);
	if (result.data) result.dynamic = true;
	return result;
}

/* ------------------------------------------------------------------ */
/*  Destruction                                                        */
/* ------------------------------------------------------------------ */

void
ring_free(ring_t *ring)
{
	if (!ring) return;
	*ring = (ring_t){0};
}

/* ------------------------------------------------------------------ */
/*  Inspection                                                         */
/* ------------------------------------------------------------------ */

size_t
ring_len(ring_t ring)
{
	return ring.len;
}

size_t
ring_capacity(ring_t ring)
{
	return ring.capacity;
}

bool
ring_is_empty(ring_t ring)
{
	return ring.len == 0;
}

bool
ring_is_full(ring_t ring)
{
	return ring.len == ring.capacity;
}

/* ------------------------------------------------------------------ */
/*  Operations                                                         */
/* ------------------------------------------------------------------ */

bool
ring_push(ring_t *ring, const void *element)
{
	if (!ring || !element) return false;

	if (ring->len == ring->capacity) {
		if (ring->dynamic) {
			if (!ring_grow(ring)) return false;
		} else {
			return false;
		}
	}

	memcpy(ring->data + ring->tail * ring->element_size,
	       element, ring->element_size);
	ring->tail = (ring->tail + 1) % ring->capacity;
	ring->len++;
	return true;
}

bool
ring_pop(ring_t *ring, void *out)
{
	if (!ring || !out || ring->len == 0) return false;

	memcpy(out,
	       ring->data + ring->head * ring->element_size,
	       ring->element_size);
	ring->head = (ring->head + 1) % ring->capacity;
	ring->len--;
	return true;
}

void *
ring_front(ring_t ring)
{
	if (ring.len == 0 || !ring.data) return NULL;
	return ring.data + ring.head * ring.element_size;
}

void *
ring_back(ring_t ring)
{
	size_t idx;

	if (ring.len == 0 || !ring.data) return NULL;

	idx = (ring.tail == 0) ? ring.capacity - 1 : ring.tail - 1;
	return ring.data + idx * ring.element_size;
}

void
ring_clear(ring_t *ring)
{
	if (!ring) return;
	ring->head = 0;
	ring->tail = 0;
	ring->len  = 0;
}
