#ifndef QUASAR_CORE_STR_H
#define QUASAR_CORE_STR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Quasar string type.
 *
 * Strings are null-terminated so that str_cstr() always returns a
 * valid C string.  The length is stored explicitly (str_len() is O(1))
 * and does not include the null terminator.
 *
 * Strings are allocated through Quasar allocators.  The allocator owns
 * the memory; the string is invalid after the allocator is freed.
 *
 * A zeroed str_t ({NULL, 0}) represents an invalid / allocation-failed
 * string.
 */
typedef struct {
	char   *data;
	size_t  len;
} str_t;

/*
 * Sentinel returned by str_index_of() and str_last_index_of() when
 * the needle is not found.
 */
#define STR_NPOS ((size_t)-1)

#ifdef __cplusplus
}
#endif

#endif
