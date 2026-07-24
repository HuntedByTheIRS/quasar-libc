#ifndef QUASAR_STD_STRINGS_H
#define QUASAR_STD_STRINGS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include <quasar/core/str.h>
#include <quasar/std/memory.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * strings.h — Quasar standard string API.
 *
 * All functions that create or transform strings accept an Allocator *
 * as their first parameter.  The allocator owns the resulting memory;
 * the returned str_t is valid only as long as the allocator is alive.
 *
 * Functions that return str_t return a zeroed struct ({NULL, 0}) on
 * allocation failure.
 *
 * Index-finding functions return STR_NPOS when the needle is not found.
 *
 * All positions and lengths are measured in bytes.  The base API does
 * not assume a particular character encoding.
 */

/*
 * ── Creation ───────────────────────────────────────────────────────
 *
 * Creation functions produce a new str_t using the provided allocator.
 * The source data is copied; the caller retains ownership of the input.
 */

/*
 * Create a string from a null-terminated C string.  The contents are
 * copied into memory owned by the allocator.
 *
 * If cstr is NULL, returns an empty string.
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_from(Allocator *a, const char *cstr);

/*
 * Create a string from a buffer of len bytes.  The input does not
 * need to be null-terminated.  The resulting string is always
 * null-terminated.
 *
 * Returns {NULL, 0} if a is NULL, data is NULL and len > 0,
 * or allocation fails.
 */
str_t str_from_n(Allocator *a, const char *data, size_t len);

/*
 * Create an empty string (len == 0, valid null-terminated data).
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_empty(Allocator *a);

/*
 * Create an independent copy of s.  The copy shares no memory with
 * the original.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_dup(Allocator *a, str_t s);

/*
 * Create a copy containing up to the first n bytes of s.  If n
 * exceeds str_len(s), the entire string is copied.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_dup_n(Allocator *a, str_t s, size_t n);

/*
 * Create a formatted string (printf-style).  The result is
 * allocated using the provided allocator.
 *
 * Returns {NULL, 0} if a is NULL, fmt is NULL, or allocation fails.
 */
str_t str_fmt(Allocator *a, const char *fmt, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
;

/*
 * Equivalent to str_fmt() but accepts an existing va_list.
 * Intended for wrapper functions that forward variadic arguments.
 *
 * Returns {NULL, 0} if a is NULL, fmt is NULL, or allocation fails.
 */
str_t str_fmt_va(Allocator *a, const char *fmt, va_list ap);

/*
 * ── Inspection / Query ─────────────────────────────────────────────
 *
 * Inspection functions retrieve information about an existing string.
 * They do not allocate and are always safe to call (including on
 * zeroed / invalid strings).
 */

/*
 * Return the length of the string in bytes.  O(1) — obtained from
 * the stored length field.
 */
size_t str_len(str_t s);

/*
 * Return true if the string has zero length (or is invalid).
 */
bool str_is_empty(str_t s);

/*
 * Return a null-terminated C string suitable for passing to
 * standard library functions.  The returned pointer is owned by
 * the string's allocator.
 *
 * Returns "" for a zeroed / invalid string (never NULL).
 */
const char *str_cstr(str_t s);

/*
 * Return a pointer to the string's underlying data.  The pointer
 * is owned by the string's allocator and must not be freed directly.
 */
const char *str_data(str_t s);

/*
 * Return true if s begins with prefix.  Two empty strings are
 * considered to start with each other.
 */
bool str_starts_with(str_t s, str_t prefix);

/*
 * Return true if s ends with suffix.  Two empty strings are
 * considered to end with each other.
 */
bool str_ends_with(str_t s, str_t suffix);

/*
 * Return true if needle occurs anywhere within s.
 * An empty needle is considered contained in any string.
 */
bool str_contains(str_t s, str_t needle);

/*
 * Return true if the string contains character c.
 */
bool str_contains_char(str_t s, char c);

/*
 * Return the byte index of the first occurrence of needle within s,
 * or STR_NPOS if not found.
 */
size_t str_index_of(str_t s, str_t needle);

/*
 * Return the byte index of the last occurrence of needle within s,
 * or STR_NPOS if not found.
 */
size_t str_last_index_of(str_t s, str_t needle);

/*
 * ── Comparison ─────────────────────────────────────────────────────
 */

/*
 * Return true if a and b have identical contents and length.
 */
bool str_equals(str_t a, str_t b);

/*
 * Return true if a and b are equal ignoring character case.
 * Comparison is ASCII-only (A-Z vs a-z).
 */
bool str_equals_icase(str_t a, str_t b);

/*
 * Lexicographical comparison.
 *
 * Returns:
 *   < 0  if a precedes b
 *     0  if a equals b
 *   > 0  if a follows b
 */
int str_compare(str_t a, str_t b);

/*
 * Case-insensitive lexicographical comparison (ASCII-only).
 * Return convention matches str_compare().
 */
int str_compare_icase(str_t a, str_t b);

/*
 * Compare up to the first n bytes of a and b.
 * Return convention matches str_compare().
 */
int str_compare_n(str_t a, str_t b, size_t n);

/*
 * ── Building / Concatenation ───────────────────────────────────────
 *
 * Building functions create new strings from existing strings or
 * character data.  The result is allocated through the provided
 * allocator.
 */

/*
 * Concatenate x and y into a new string.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_cat(Allocator *a, str_t x, str_t y);

/*
 * Concatenate s with a null-terminated C string.
 *
 * If cstr is NULL, returns a copy of s.
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_cat_cstr(Allocator *a, str_t s, const char *cstr);

/*
 * Append a single character to s.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_cat_char(Allocator *a, str_t s, char c);

/*
 * Join an array of strings with separator between each element.
 *
 * Returns {NULL, 0} if a is NULL, strings is NULL, or allocation fails.
 */
str_t str_join(Allocator *a, str_t *strings, size_t count, str_t separator);

/*
 * Prepend prefix before s.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_prepend(Allocator *a, str_t s, str_t prefix);

/*
 * Append suffix after s.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_append(Allocator *a, str_t s, str_t suffix);

/*
 * ── Substring & Slicing ────────────────────────────────────────────
 *
 * All positions and lengths are in bytes.  Functions clamp
 * out-of-range parameters to valid bounds.
 */

/*
 * Create a string from the byte range [start, end).  end is exclusive.
 * Out-of-range values are clamped to [0, str_len(s)].
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_slice(Allocator *a, str_t s, size_t start, size_t end);

/*
 * Create a string beginning at start and containing up to len bytes.
 * Out-of-range values are clamped.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_substr(Allocator *a, str_t s, size_t start, size_t len);

/*
 * Create a string containing the first n bytes of s.  If n exceeds
 * str_len(s), the entire string is returned.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_head(Allocator *a, str_t s, size_t n);

/*
 * Create a string containing the last n bytes of s.  If n exceeds
 * str_len(s), the entire string is returned.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_tail(Allocator *a, str_t s, size_t n);

/*
 * Remove prefix from s if s begins with prefix, otherwise return
 * a copy of s unchanged.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_remove_prefix(Allocator *a, str_t s, str_t prefix);

/*
 * Remove suffix from s if s ends with suffix, otherwise return
 * a copy of s unchanged.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_remove_suffix(Allocator *a, str_t s, str_t suffix);

/*
 * ── Transformation ─────────────────────────────────────────────────
 *
 * Transformation functions create new strings containing modified
 * versions of the input.  The original string is never mutated.
 */

/*
 * Create a string with leading and trailing whitespace removed.
 * Whitespace characters: ' ', '\t', '\n', '\r', '\v', '\f'.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_trim(Allocator *a, str_t s);

/*
 * Create a string with leading whitespace removed.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_trim_left(Allocator *a, str_t s);

/*
 * Create a string with trailing whitespace removed.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_trim_right(Allocator *a, str_t s);

/*
 * Create a lowercase version of the string (ASCII-only: A-Z → a-z).
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_to_lower(Allocator *a, str_t s);

/*
 * Create an uppercase version of the string (ASCII-only: a-z → A-Z).
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_to_upper(Allocator *a, str_t s);

/*
 * Create a new string with occurrences of old replaced by new_str.
 * Overlapping occurrences are not re-scanned (non-overlapping
 * replacement).
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_replace(Allocator *a, str_t s, str_t old, str_t new_str);

/*
 * Create a reversed version of the string.  The reversal operates on
 * the byte representation (not Unicode-aware).
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_reverse(Allocator *a, str_t s);

/*
 * ── Search & Count ─────────────────────────────────────────────────
 */

/*
 * Return the number of non-overlapping occurrences of needle in
 * haystack.  Returns 0 when needle is empty (avoiding infinite
 * counting).
 */
size_t str_count(str_t haystack, str_t needle);

/*
 * Split s into an array of strings using delimiter.  The resulting
 * array and its elements are allocated using the provided allocator.
 * The number of elements is written to *out_count.
 *
 * An empty string split by any delimiter yields a single-element
 * array containing an empty string.  An empty delimiter splits s
 * into individual characters.
 *
 * Returns NULL if a is NULL, out_count is NULL, delimiter data is
 * NULL, or allocation fails.  On failure, *out_count is set to 0.
 * The caller is responsible for freeing the allocator to reclaim
 * any partially-allocated memory.
 */
str_t *str_split(Allocator *a, str_t s, str_t delimiter,
		 size_t *out_count);

/*
 * Create a string by repeating s n times.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_repeat(Allocator *a, str_t s, size_t n);

/*
 * Return true if the string contains only whitespace characters.
 * An empty string is considered empty, not whitespace-only.
 */
bool str_is_whitespace(str_t s);

/*
 * ── Utility ────────────────────────────────────────────────────────
 */

/*
 * Create a copy of s.  Alias for str_dup().
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t str_copy(Allocator *a, str_t s);

/*
 * Clear the contents of a string in place by setting its length
 * to zero and null-terminating the existing buffer at position 0.
 * The allocated memory is not freed and may be reused.
 */
void str_clear(str_t *s);

/*
 * Return a hash of the string's contents, suitable for use with
 * hash tables.  Equal strings produce equal hashes within the
 * same process.
 */
size_t str_hash(str_t s);

#ifdef __cplusplus
}
#endif

#endif
