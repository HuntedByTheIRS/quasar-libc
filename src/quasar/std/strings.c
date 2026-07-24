#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../core/mem.h"
#include <quasar/core/str.h>
#include <quasar/std/strings.h>

/*
 * Allocate len + 1 bytes from the allocator and null-terminate.
 * Returns NULL if allocation fails or a is NULL.
 */
static char *_str_alloc(Allocator *a, size_t len)
{
	char *data;

	if (!a) return 0;
	if (len == (size_t)-1) return 0;
	data = _quasar_mem_allocate(a, len + 1);
	if (data) data[len] = '\0';
	return data;
}

/* ------------------------------------------------------------------ */
/*  Creation                                                           */
/* ------------------------------------------------------------------ */

str_t
str_from_n(Allocator *a, const char *data, size_t len)
{
	str_t result = {0, 0};

	if (!a) return result;
	if (!data && len > 0) return result;
	result.data = _str_alloc(a, len);
	if (!result.data) return result;
	if (len > 0) memcpy(result.data, data, len);
	result.len = len;
	return result;
}

str_t
str_empty(Allocator *a)
{
	return str_from_n(a, "", 0);
}

str_t
str_from(Allocator *a, const char *cstr)
{
	if (!cstr) return str_empty(a);
	return str_from_n(a, cstr, strlen(cstr));
}

str_t
str_dup(Allocator *a, str_t s)
{
	if (!s.data) return str_empty(a);
	return str_from_n(a, s.data, s.len);
}

str_t
str_dup_n(Allocator *a, str_t s, size_t n)
{
	if (!s.data) return str_empty(a);
	if (n > s.len) n = s.len;
	return str_from_n(a, s.data, n);
}

str_t
str_fmt_va(Allocator *a, const char *fmt, va_list ap)
{
	va_list ap2;
	int     needed;
	char   *buf;

	if (!a || !fmt) {
		str_t empty = {0, 0};
		return empty;
	}
	va_copy(ap2, ap);
	needed = vsnprintf(0, 0, fmt, ap2);
	va_end(ap2);
	if (needed < 0) return str_empty(a);
	buf = _str_alloc(a, (size_t)needed);
	if (!buf) {
		str_t empty = {0, 0};
		return empty;
	}
	vsnprintf(buf, (size_t)needed + 1, fmt, ap);
	return (str_t){buf, (size_t)needed};
}

str_t
str_fmt(Allocator *a, const char *fmt, ...)
{
	va_list ap;
	str_t   result;

	va_start(ap, fmt);
	result = str_fmt_va(a, fmt, ap);
	va_end(ap);
	return result;
}

/* ------------------------------------------------------------------ */
/*  Inspection / Query                                                 */
/* ------------------------------------------------------------------ */

size_t
str_len(str_t s)
{
	return s.len;
}

bool
str_is_empty(str_t s)
{
	return s.len == 0 || !s.data;
}

const char *
str_cstr(str_t s)
{
	return s.data ? s.data : "";
}

const char *
str_data(str_t s)
{
	return s.data;
}

bool
str_starts_with(str_t s, str_t prefix)
{
	if (prefix.len == 0) return true;
	if (!s.data || !prefix.data) return false;
	if (prefix.len > s.len) return false;
	return memcmp(s.data, prefix.data, prefix.len) == 0;
}

bool
str_ends_with(str_t s, str_t suffix)
{
	if (suffix.len == 0) return true;
	if (!s.data || !suffix.data) return false;
	if (suffix.len > s.len) return false;
	return memcmp(s.data + s.len - suffix.len,
		      suffix.data, suffix.len) == 0;
}

/*
 * Naive substring search.  For needle lengths > 0, slide a window
 * across the haystack and compare byte-by-byte.
 */
static size_t
_str_find(str_t haystack, str_t needle, size_t start)
{
	size_t i;

	if (needle.len == 0) return start <= haystack.len ? start : STR_NPOS;
	if (needle.len > haystack.len) return STR_NPOS;
	for (i = start; i <= haystack.len - needle.len; i++) {
		if (memcmp(haystack.data + i, needle.data, needle.len) == 0)
			return i;
	}
	return STR_NPOS;
}

size_t
str_index_of(str_t s, str_t needle)
{
	if (needle.len == 0) return 0;
	if (!s.data || !needle.data)
		return STR_NPOS;
	return _str_find(s, needle, 0);
}

size_t
str_last_index_of(str_t s, str_t needle)
{
	size_t i;

	if (needle.len == 0)
		return s.len;
	if (!s.data || !needle.data)
		return STR_NPOS;
	if (needle.len > s.len)
		return STR_NPOS;
	/*
	 * Walk backwards so we return the last match immediately.
	 */
	i = s.len - needle.len;
	do {
		if (memcmp(s.data + i, needle.data, needle.len) == 0)
			return i;
	} while (i-- > 0);
	return STR_NPOS;
}

bool
str_contains(str_t s, str_t needle)
{
	return str_index_of(s, needle) != STR_NPOS;
}

bool
str_contains_char(str_t s, char c)
{
	if (!s.data || s.len == 0) return false;
	return memchr(s.data, c, s.len) != 0;
}

/* ------------------------------------------------------------------ */
/*  Comparison                                                         */
/* ------------------------------------------------------------------ */

bool
str_equals(str_t a, str_t b)
{
	if (a.len != b.len) return false;
	if (!a.data && !b.data) return true;
	if (!a.data || !b.data) return false;
	return memcmp(a.data, b.data, a.len) == 0;
}

bool
str_equals_icase(str_t a, str_t b)
{
	size_t i;

	if (a.len != b.len) return false;
	if (!a.data && !b.data) return true;
	if (!a.data || !b.data) return false;
	for (i = 0; i < a.len; i++) {
		char ca = a.data[i];
		char cb = b.data[i];
		if (ca >= 'A' && ca <= 'Z') ca = (char)(ca | 0x20);
		if (cb >= 'A' && cb <= 'Z') cb = (char)(cb | 0x20);
		if (ca != cb)
			return false;
	}
	return true;
}

int
str_compare(str_t a, str_t b)
{
	size_t minlen;
	int    cmp;

	if (!a.data && !b.data) return 0;
	if (!a.data) return -1;
	if (!b.data) return 1;
	minlen = a.len < b.len ? a.len : b.len;
	cmp = memcmp(a.data, b.data, minlen);
	if (cmp != 0) return cmp;
	if (a.len < b.len) return -1;
	if (a.len > b.len) return 1;
	return 0;
}

int
str_compare_icase(str_t a, str_t b)
{
	size_t i;
	int    diff;

	if (!a.data && !b.data) return 0;
	if (!a.data) return -1;
	if (!b.data) return 1;
	for (i = 0; i < a.len && i < b.len; i++) {
		char ca = a.data[i];
		char cb = b.data[i];
		if (ca >= 'A' && ca <= 'Z') ca = (char)(ca | 0x20);
		if (cb >= 'A' && cb <= 'Z') cb = (char)(cb | 0x20);
		diff = ca - cb;
		if (diff != 0) return diff;
	}
	if (a.len < b.len) return -1;
	if (a.len > b.len) return 1;
	return 0;
}

int
str_compare_n(str_t a, str_t b, size_t n)
{
	size_t minlen;
	int    cmp;

	if (!a.data && !b.data) return 0;
	if (!a.data) return -1;
	if (!b.data) return 1;
	minlen = a.len < b.len ? a.len : b.len;
	if (n < minlen) minlen = n;
	cmp = memcmp(a.data, b.data, minlen);
	if (cmp != 0) return cmp;
	if (n <= a.len && n <= b.len) return 0;
	if (a.len < b.len) return -1;
	if (a.len > b.len) return 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Building / Concatenation                                           */
/* ------------------------------------------------------------------ */

str_t
str_cat(Allocator *a, str_t x, str_t y)
{
	size_t total;
	char  *buf;

	if (!a) {
		str_t empty = {0, 0};
		return empty;
	}
	if (x.len > (size_t)-1 - y.len) {
		str_t empty = {0, 0};
		return empty;
	}
	total = x.len + y.len;
	buf = _str_alloc(a, total);
	if (!buf) {
		str_t empty = {0, 0};
		return empty;
	}
	if (x.len > 0) memcpy(buf, x.data, x.len);
	if (y.len > 0) memcpy(buf + x.len, y.data, y.len);
	return (str_t){buf, total};
}

str_t
str_cat_cstr(Allocator *a, str_t s, const char *cstr)
{
	size_t clen;

	if (!a) {
		str_t empty = {0, 0};
		return empty;
	}
	if (!cstr) return str_dup(a, s);
	clen = strlen(cstr);
	if (s.len > (size_t)-1 - clen) {
		str_t empty = {0, 0};
		return empty;
	}
	{
		char  *buf;
		size_t total = s.len + clen;

		buf = _str_alloc(a, total);
		if (!buf) {
			str_t empty = {0, 0};
			return empty;
		}
		if (s.len > 0) memcpy(buf, s.data, s.len);
		memcpy(buf + s.len, cstr, clen);
		return (str_t){buf, total};
	}
}

str_t
str_cat_char(Allocator *a, str_t s, char c)
{
	char  *buf;

	if (!a) {
		str_t empty = {0, 0};
		return empty;
	}
	if (s.len == (size_t)-1) {
		str_t empty = {0, 0};
		return empty;
	}
	buf = _str_alloc(a, s.len + 1);
	if (!buf) {
		str_t empty = {0, 0};
		return empty;
	}
	if (s.len > 0) memcpy(buf, s.data, s.len);
	buf[s.len] = c;
	return (str_t){buf, s.len + 1};
}

str_t
str_join(Allocator *a, str_t *strings, size_t count, str_t separator)
{
	size_t total;
	size_t i;
	char  *buf;
	char  *dst;

	if (!a || !strings)
		return (str_t){0, 0};
	if (count == 0)
		return str_empty(a);
	total = 0;
	for (i = 0; i < count; i++) {
		if (strings[i].len > (size_t)-1 - total) {
			str_t empty = {0, 0};
			return empty;
		}
		total += strings[i].len;
	}
	if (count > 1) {
		size_t sep_total;

		if (separator.len > 0 &&
		    (count - 1) > (size_t)-1 / separator.len) {
			str_t empty = {0, 0};
			return empty;
		}
		sep_total = separator.len * (count - 1);
		if (sep_total > (size_t)-1 - total) {
			str_t empty = {0, 0};
			return empty;
		}
		total += sep_total;
	}
	buf = _str_alloc(a, total);
	if (!buf) {
		str_t empty = {0, 0};
		return empty;
	}
	dst = buf;
	for (i = 0; i < count; i++) {
		if (i > 0 && separator.len > 0) {
			memcpy(dst, separator.data, separator.len);
			dst += separator.len;
		}
		if (strings[i].len > 0) {
			memcpy(dst, strings[i].data, strings[i].len);
			dst += strings[i].len;
		}
	}
	return (str_t){buf, total};
}

str_t
str_prepend(Allocator *a, str_t s, str_t prefix)
{
	return str_cat(a, prefix, s);
}

str_t
str_append(Allocator *a, str_t s, str_t suffix)
{
	return str_cat(a, s, suffix);
}

/* ------------------------------------------------------------------ */
/*  Substring & Slicing                                                */
/* ------------------------------------------------------------------ */

str_t
str_slice(Allocator *a, str_t s, size_t start, size_t end)
{
	if (!s.data) return str_empty(a);
	if (start > s.len) start = s.len;
	if (end > s.len) end = s.len;
	if (start >= end) return str_empty(a);
	return str_from_n(a, s.data + start, end - start);
}

str_t
str_substr(Allocator *a, str_t s, size_t start, size_t len)
{
	if (!s.data) return str_empty(a);
	if (start > s.len) start = s.len;
	{
		size_t remaining = s.len - start;
		if (len > remaining) len = remaining;
	}
	if (len == 0) return str_empty(a);
	return str_from_n(a, s.data + start, len);
}

str_t
str_head(Allocator *a, str_t s, size_t n)
{
	if (n >= s.len) return str_dup(a, s);
	return str_slice(a, s, 0, n);
}

str_t
str_tail(Allocator *a, str_t s, size_t n)
{
	if (n >= s.len) return str_dup(a, s);
	return str_slice(a, s, s.len - n, s.len);
}

str_t
str_remove_prefix(Allocator *a, str_t s, str_t prefix)
{
	if (!s.data) return str_empty(a);
	if (str_starts_with(s, prefix))
		return str_slice(a, s, prefix.len, s.len);
	return str_dup(a, s);
}

str_t
str_remove_suffix(Allocator *a, str_t s, str_t suffix)
{
	if (!s.data) return str_empty(a);
	if (str_ends_with(s, suffix))
		return str_slice(a, s, 0, s.len - suffix.len);
	return str_dup(a, s);
}

/* ------------------------------------------------------------------ */
/*  Transformation                                                     */
/* ------------------------------------------------------------------ */

/*
 * Return true if byte c is an ASCII whitespace character.
 */
static bool
_str_is_ws(char c)
{
	return c == ' ' || c == '\t' || c == '\n' ||
	       c == '\r' || c == '\v' || c == '\f';
}

str_t
str_trim(Allocator *a, str_t s)
{
	size_t start, end;

	if (!s.data || s.len == 0) return str_empty(a);
	start = 0;
	while (start < s.len && _str_is_ws(s.data[start]))
		start++;
	end = s.len;
	while (end > start && _str_is_ws(s.data[end - 1]))
		end--;
	return str_slice(a, s, start, end);
}

str_t
str_trim_left(Allocator *a, str_t s)
{
	size_t start;

	if (!s.data || s.len == 0) return str_empty(a);
	start = 0;
	while (start < s.len && _str_is_ws(s.data[start]))
		start++;
	return str_slice(a, s, start, s.len);
}

str_t
str_trim_right(Allocator *a, str_t s)
{
	size_t end;

	if (!s.data || s.len == 0) return str_empty(a);
	end = s.len;
	while (end > 0 && _str_is_ws(s.data[end - 1]))
		end--;
	return str_slice(a, s, 0, end);
}

str_t
str_to_lower(Allocator *a, str_t s)
{
	char  *buf;
	size_t i;

	if (!s.data) return str_empty(a);
	buf = _str_alloc(a, s.len);
	if (!buf) {
		str_t empty = {0, 0};
		return empty;
	}
	for (i = 0; i < s.len; i++) {
		char c = s.data[i];
		buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c | 0x20) : c;
	}
	return (str_t){buf, s.len};
}

str_t
str_to_upper(Allocator *a, str_t s)
{
	char  *buf;
	size_t i;

	if (!s.data) return str_empty(a);
	buf = _str_alloc(a, s.len);
	if (!buf) {
		str_t empty = {0, 0};
		return empty;
	}
	for (i = 0; i < s.len; i++) {
		char c = s.data[i];
		buf[i] = (c >= 'a' && c <= 'z') ? (char)(c & ~0x20) : c;
	}
	return (str_t){buf, s.len};
}

str_t
str_replace(Allocator *a, str_t s, str_t old, str_t new_str)
{
	size_t count;
	size_t i, pos;
	size_t total;
	char  *buf;
	char  *dst;

	if (!a) {
		str_t empty = {0, 0};
		return empty;
	}
	if (!s.data)
		return str_empty(a);
	if (old.len == 0 || !old.data)
		return str_dup(a, s);
	/* Count occurrences (non-overlapping). */
	count = 0;
	for (i = 0; i + old.len <= s.len; i++) {
		if (memcmp(s.data + i, old.data, old.len) == 0) {
			count++;
			i += old.len - 1;
		}
	}
	if (count == 0) return str_dup(a, s);
	/*
	 * Compute total length.  Use signed arithmetic to avoid
	 * unsigned wraparound when new_str.len < old.len.
	 */
	if (new_str.len >= old.len) {
		size_t growth = new_str.len - old.len;

		if (growth > 0 && count > (size_t)-1 / growth) {
			str_t empty = {0, 0};
			return empty;
		}
		if (count * growth > (size_t)-1 - s.len) {
			str_t empty = {0, 0};
			return empty;
		}
		total = s.len + count * growth;
	} else {
		size_t shrink = old.len - new_str.len;

		total = s.len - count * shrink;
	}
	buf = _str_alloc(a, total);
	if (!buf) {
		str_t empty = {0, 0};
		return empty;
	}
	/* Build result. */
	dst = buf;
	pos = 0;
	while (pos < s.len) {
		if (pos + old.len <= s.len &&
		    memcmp(s.data + pos, old.data, old.len) == 0) {
			if (new_str.len > 0) {
				memcpy(dst, new_str.data, new_str.len);
				dst += new_str.len;
			}
			pos += old.len;
		} else {
			*dst++ = s.data[pos++];
		}
	}
	return (str_t){buf, total};
}

str_t
str_reverse(Allocator *a, str_t s)
{
	char  *buf;
	size_t i;

	if (!s.data) return str_empty(a);
	buf = _str_alloc(a, s.len);
	if (!buf) {
		str_t empty = {0, 0};
		return empty;
	}
	for (i = 0; i < s.len; i++)
		buf[i] = s.data[s.len - 1 - i];
	return (str_t){buf, s.len};
}

/* ------------------------------------------------------------------ */
/*  Search & Count                                                     */
/* ------------------------------------------------------------------ */

size_t
str_count(str_t haystack, str_t needle)
{
	size_t count;
	size_t i;

	if (!haystack.data || !needle.data) return 0;
	if (needle.len == 0) return 0;
	if (needle.len > haystack.len) return 0;
	count = 0;
	for (i = 0; i + needle.len <= haystack.len; i++) {
		if (memcmp(haystack.data + i, needle.data, needle.len) == 0) {
			count++;
			i += needle.len - 1;
		}
	}
	return count;
}

str_t *
str_split(Allocator *a, str_t s, str_t delimiter, size_t *out_count)
{
	size_t  count;
	size_t  i, start;
	str_t  *result;
	size_t  ri;

	if (!a || !out_count) return 0;
	*out_count = 0;
	if (!s.data) {
		result = _quasar_mem_allocate(a, sizeof(str_t));
		if (!result) return 0;
		result[0] = str_empty(a);
		*out_count = 1;
		return result;
	}
	/*
	 * Count pieces.  Each delimiter starts a new piece.  If the
	 * delimiter is empty, return individual characters (safety
	 * fallback).
	 */
	if (!delimiter.data) return 0;
	if (delimiter.len == 0) {
		/* Empty string: single-element array per spec. */
		if (s.len == 0) {
			result = _quasar_mem_allocate(a, sizeof(str_t));
			if (!result) return 0;
			result[0] = str_empty(a);
			*out_count = 1;
			return result;
		}
		/* Split into individual characters. */
		if (s.len > (size_t)-1 / sizeof(str_t))
			return 0;
		result = _quasar_mem_allocate(a,
					       s.len * sizeof(str_t));
		if (!result) return 0;
		for (i = 0; i < s.len; i++) {
			char *ch = _str_alloc(a, 1);
			if (!ch) return 0;
			ch[0] = s.data[i];
			result[i] = (str_t){ch, 1};
		}
		*out_count = s.len;
		return result;
	}
	/* Count pieces. */
	count = 1;
	for (i = 0; i + delimiter.len <= s.len; i++) {
		if (memcmp(s.data + i, delimiter.data,
			   delimiter.len) == 0) {
			count++;
			i += delimiter.len - 1;
		}
	}
	/* Allocate array. */
	result = _quasar_mem_allocate(a, count * sizeof(str_t));
	if (!result) return 0;
	/* Fill array. */
	ri = 0;
	start = 0;
	i = 0;
	while (i < s.len) {
		if (i + delimiter.len <= s.len &&
		    memcmp(s.data + i, delimiter.data,
			   delimiter.len) == 0) {
			result[ri++] = str_from_n(a, s.data + start,
						   i - start);
			i += delimiter.len;
			start = i;
		} else {
			i++;
		}
	}
	/* Last piece. */
	result[ri++] = str_from_n(a, s.data + start, s.len - start);
	*out_count = count;
	return result;
}

str_t
str_repeat(Allocator *a, str_t s, size_t n)
{
	char  *buf;
	size_t i;

	if (!a) {
		str_t empty = {0, 0};
		return empty;
	}
	if (n == 0 || !s.data || s.len == 0)
		return str_empty(a);
	if (n > (size_t)-1 / s.len) {
		str_t empty = {0, 0};
		return empty;
	}
	buf = _str_alloc(a, s.len * n);
	if (!buf) {
		str_t empty = {0, 0};
		return empty;
	}
	for (i = 0; i < n; i++)
		memcpy(buf + i * s.len, s.data, s.len);
	return (str_t){buf, s.len * n};
}

bool
str_is_whitespace(str_t s)
{
	size_t i;

	if (str_is_empty(s)) return false;
	for (i = 0; i < s.len; i++) {
		if (!_str_is_ws(s.data[i]))
			return false;
	}
	return true;
}

/* ------------------------------------------------------------------ */
/*  Utility                                                            */
/* ------------------------------------------------------------------ */

str_t
str_copy(Allocator *a, str_t s)
{
	return str_dup(a, s);
}

void
str_clear(str_t *s)
{
	if (!s) return;
	s->len = 0;
	if (s->data) s->data[0] = '\0';
}

/*
 * FNV-1a hash.  Uses 64-bit constants on 64-bit platforms.
 */
size_t
str_hash(str_t s)
{
	size_t hash;
	size_t i;

	if (!s.data) {
#if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ >= 8
		return 14695981039346656037ULL;
#else
		return 2166136261U;
#endif
	}
#if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ >= 8
	hash = 14695981039346656037ULL;
	for (i = 0; i < s.len; i++) {
		hash ^= (unsigned char)s.data[i];
		hash *= 1099511628211ULL;
	}
#else
	hash = 2166136261U;
	for (i = 0; i < s.len; i++) {
		hash ^= (unsigned char)s.data[i];
		hash *= 16777619U;
	}
#endif
	return hash;
}
