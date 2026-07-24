
# std/strings.h

`strings.h` is a part of the Quasar standard library intended to provide a high-level string interface for C.

The Quasar string system is designed to provide common string functionality without requiring users to repeatedly implement manual allocation, resizing, copying, concatenation, searching, and transformation logic.

Strings are allocated through Quasar allocators. The allocator passed to a string creation or transformation function determines the lifetime and ownership of the resulting string.

Quasar strings are represented by `str_t`.

---

## Ownership

Strings created through an allocator are owned by the allocator used to create them.

The lifetime of a string is therefore determined by the lifetime and behavior of its allocator.

For example:

```c
Allocator *a = mem_allocators_dynamic();

str_t s = str_from(a, "Hello, Quasar!");

mem_allocator_free(a);

// s is no longer valid.
```

The string does not own its allocator.

The allocator remains responsible for the memory used by the string.

Higher-level string operations that create new strings require an `Allocator *` so that the caller can explicitly choose the lifetime and allocation strategy of the resulting data.

For example:

```c
Allocator *arena = mem_allocators_arena();

str_t a = str_from(arena, "Hello");
str_t b = str_from(arena, "Quasar");

str_t c = str_cat(arena, a, b);

mem_allocator_free(arena);
```

All three strings share the lifetime of the arena.

---

## String Representation

`str_t` represents a string using its data and length.

The length of a string is stored explicitly rather than requiring string operations to repeatedly calculate it.

The string's data may be accessed through `str_data()`.

C-string compatibility is provided through `str_cstr()`.

The exact representation of `str_t` is an implementation detail unless otherwise specified by the public interface.

Users should generally interact with strings through the functions provided by `strings.h`.

---

# Creation

Creation functions create strings using a caller-provided allocator.

### `str_t str_from(Allocator *a, const char *cstr)`

Creates a string from a null-terminated C string.

The contents of the C string are copied into memory owned by the provided allocator.

#### Example

```c
Allocator *a = mem_allocators_dynamic();

str_t s = str_from(a, "Hello, Quasar!");
```

The resulting string is owned by `a`.

The original C string is not owned by the resulting `str_t`.

---

### `str_t str_from_n(Allocator *a, const char *data, size_t len)`

Creates a string from a data buffer containing `len` bytes.

Unlike `str_from()`, this function does not require the input data to be null-terminated.

This function is intended for data where the length is already known or where the data may not be terminated by a null byte.

#### Example

```c
Allocator *a = mem_allocators_dynamic();

const char data[] = {
 'H', 'e', 'l', 'l', 'o'
};

str_t s = str_from_n(a, data, 5);
```

The resulting string is owned by `a`.

---

### `str_t str_empty(Allocator *a)`

Creates an empty string owned by the provided allocator.

The resulting string has a length of zero.

#### Example

```c
Allocator *a = mem_allocators_dynamic();

str_t s = str_empty(a);

if (str_is_empty(s)) {
 /* string is empty */
}
```

---

### `str_t str_dup(Allocator *a, str_t s)`

Creates a copy of an existing string using the provided allocator.

The resulting string is independent of the original string's storage.

#### Example

```c
Allocator *a = mem_allocators_dynamic();

str_t copy = str_dup(a, original);
```

---

### `str_t str_dup_n(Allocator *a, str_t s, size_t n)`

Creates a copy containing up to the first `n` bytes of an existing string.

If `n` is greater than the length of `s`, the resulting string contains the entire source string.

---

### `str_t str_fmt(Allocator *a, const char *fmt, ...)`

Creates a formatted string using a `printf`-style format string.

The resulting string is allocated using the provided allocator.

#### Example

```c
Allocator *a = mem_allocators_dynamic();

str_t s = str_fmt(a, "The answer is %d", 42);
```

---

### `str_t str_fmt_va(Allocator *a, const char *fmt, va_list ap)`

Equivalent to `str_fmt()`, but accepts an existing `va_list`.

This function is intended primarily for use by functions that forward variadic arguments.

---

# Inspection / Query

Inspection functions retrieve information about an existing string without creating a new string.

### `size_t str_len(str_t s)`

Returns the length of the string in bytes.

The length is obtained from the string's stored length rather than requiring a scan for a null terminator.

---

### `bool str_is_empty(str_t s)`

Returns `true` if the string contains zero bytes.

---

### `const char *str_cstr(str_t s)`

Returns the string as a null-terminated C string.

This function is intended for interoperability with APIs that require standard C strings.

---

### `const char *str_data(str_t s)`

Returns a pointer to the string's underlying data.

The returned pointer is owned by the allocator associated with the string.

The pointer should not be freed directly.

---

### `bool str_starts_with(str_t s, str_t prefix)`

Returns `true` if `s` begins with `prefix`.

---

### `bool str_ends_with(str_t s, str_t suffix)`

Returns `true` if `s` ends with `suffix`.

---

### `bool str_contains(str_t s, str_t needle)`

Returns `true` if `needle` occurs anywhere within `s`.

---

### `bool str_contains_char(str_t s, char c)`

Returns `true` if the string contains the specified character.

---

### `size_t str_index_of(str_t s, str_t needle)`

Returns the byte index of the first occurrence of `needle` within `s`.

If `needle` is not found, the function returns the designated "not found" value.

---

### `size_t str_last_index_of(str_t s, str_t needle)`

Returns the byte index of the last occurrence of `needle` within `s`.

If `needle` is not found, the function returns the designated "not found" value.

---

# Comparison

Comparison functions compare string contents.

### `bool str_equals(str_t a, str_t b)`

Returns `true` if both strings contain identical data and have identical lengths.

---

### `bool str_equals_icase(str_t a, str_t b)`

Performs a case-insensitive comparison of two strings.

Returns `true` if the strings are equal without considering character case.

---

### `int str_compare(str_t a, str_t b)`

Performs a lexicographical comparison of two strings.

The return value follows the standard comparison convention:

* Less than zero if `a` precedes `b`.
* Zero if `a` and `b` are equal.
* Greater than zero if `a` follows `b`.

---

### `int str_compare_icase(str_t a, str_t b)`

Performs a case-insensitive lexicographical comparison.

The return value follows the same convention as `str_compare()`.

---

### `int str_compare_n(str_t a, str_t b, size_t n)`

Compares up to the first `n` bytes of two strings.

The return value follows the standard comparison convention.

---

# Building / Concatenation

Building functions create new strings from existing strings or character data.

The resulting strings are allocated through the provided allocator.

### `str_t str_cat(Allocator *a, str_t x, str_t y)`

Creates a new string containing `x` followed by `y`.

#### Example

```c
str_t hello = str_from(a, "Hello, ");
str_t world = str_from(a, "world!");

str_t result = str_cat(a, hello, world);
```

---

### `str_t str_cat_cstr(Allocator *a, str_t s, const char *cstr)`

Creates a new string containing `s` followed by a null-terminated C string.

---

### `str_t str_cat_char(Allocator *a, str_t s, char c)`

Creates a new string containing `s` followed by the specified character.

---

### `str_t str_join(Allocator *a, str_t *strings, size_t count, str_t separator)`

Creates a new string by joining an array of strings with a separator between each element.

#### Example

```c
str_t values[] = {
 str_from(a, "one"),
 str_from(a, "two"),
 str_from(a, "three")
};

str_t separator = str_from(a, ", ");

str_t result = str_join(a, values, 3, separator);
```

The resulting string is:

```text
one, two, three
```

---

### `str_t str_prepend(Allocator *a, str_t s, str_t prefix)`

Creates a new string containing `prefix` followed by `s`.

---

### `str_t str_append(Allocator *a, str_t s, str_t suffix)`

Creates a new string containing `s` followed by `suffix`.

---

# Substring & Slicing

Substring functions create strings containing portions of an existing string.

All positions and lengths are expressed in bytes.

### `str_t str_slice(Allocator *a, str_t s, size_t start, size_t end)`

Creates a string containing the bytes in the range `[start, end)`.

The `end` position is exclusive.

---

### `str_t str_substr(Allocator *a, str_t s, size_t start, size_t len)`

Creates a string beginning at `start` and containing up to `len` bytes.

---

### `str_t str_head(Allocator *a, str_t s, size_t n)`

Creates a string containing the first `n` bytes of `s`.

If `n` exceeds the length of `s`, the entire string is returned.

---

### `str_t str_tail(Allocator *a, str_t s, size_t n)`

Creates a string containing the last `n` bytes of `s`.

If `n` exceeds the length of `s`, the entire string is returned.

---

### `str_t str_remove_prefix(Allocator *a, str_t s, str_t prefix)`

Creates a copy of `s` with `prefix` removed if `s` begins with `prefix`.

If `s` does not begin with `prefix`, the resulting string retains the original contents.

---

### `str_t str_remove_suffix(Allocator *a, str_t s, str_t suffix)`

Creates a copy of `s` with `suffix` removed if `s` ends with `suffix`.

If `s` does not end with `suffix`, the resulting string retains the original contents.

---

# Transformation

Transformation functions create new strings containing modified versions of existing strings.

### `str_t str_trim(Allocator *a, str_t s)`

Creates a string with leading and trailing whitespace removed.

---

### `str_t str_trim_left(Allocator *a, str_t s)`

Creates a string with leading whitespace removed.

---

### `str_t str_trim_right(Allocator *a, str_t s)`

Creates a string with trailing whitespace removed.

---

### `str_t str_to_lower(Allocator *a, str_t s)`

Creates a lowercase version of the string.

---

### `str_t str_to_upper(Allocator *a, str_t s)`

Creates an uppercase version of the string.

---

### `str_t str_replace(Allocator *a, str_t s, str_t old, str_t new_str)`

Creates a new string in which occurrences of `old` are replaced with `new_str`.

---

### `str_t str_reverse(Allocator *a, str_t s)`

Creates a reversed version of the string.

The reversal operates on the string's byte representation.

---

# Search & Count

### `size_t str_count(str_t haystack, str_t needle)`

Returns the number of occurrences of `needle` within `haystack`.

---

### `str_t *str_split(Allocator *a, str_t s, str_t delimiter, size_t *out_count)`

Splits a string into an array of strings using `delimiter`.

The resulting array and strings are allocated using the provided allocator.

The number of resulting strings is written to `out_count`.

#### Example

```c
Allocator *a = mem_allocators_arena();

str_t input = str_from(a, "one,two,three");
str_t delimiter = str_from(a, ",");

size_t count;

str_t *parts = str_split(a, input, delimiter, &count);
```

Because the array and its contents share the same allocator, they can all be released together by destroying the allocator.

---

### `str_t str_repeat(Allocator *a, str_t s, size_t n)`

Creates a string containing `s` repeated `n` times.

For example:

```c
str_t s = str_from(a, "ha");

str_t result = str_repeat(a, s, 3);
```

Produces:

```text
hahaha
```

---

### `bool str_is_whitespace(str_t s)`

Returns `true` if the string contains only whitespace characters.

An empty string is considered empty rather than containing whitespace.

---

# Utility

### `str_t str_copy(Allocator *a, str_t s)`

Creates a copy of `s`.

This function is an alias for `str_dup()`.

---

### `void str_clear(str_t *s)`

Clears the contents of a string by setting its length to zero.

The string's allocated memory is not freed.

The underlying allocation remains owned by its allocator and may be reused according to the semantics of the string implementation.

#### Example

```c
str_t s = str_from(a, "Hello");

str_clear(&s);

/* s is now empty. */
```

---

### `size_t str_hash(str_t s)`

Returns a hash value derived from the contents of the string.

The hash function is intended for use with hash tables and other data structures that require string hashing.

The hash function must produce the same result for equal strings within the same Quasar implementation and hash algorithm.

---

# Allocator Integration

String creation and transformation functions accept an `Allocator *`.

This allows the same string API to work with different allocation strategies.

For example:

### Dynamic strings

```c
Allocator *a = mem_allocators_dynamic();

str_t s = str_from(a, "Long-lived string");

mem_allocator_free(a);
```

### Arena strings

```c
Allocator *arena = mem_allocators_arena();

str_t a = str_from(arena, "First");
str_t b = str_from(arena, "Second");
str_t c = str_cat(arena, a, b);

mem_allocator_free(arena);
```

### Temporary strings

```c
Allocator *temp = mem_allocators_temp();

str_t result = str_fmt(temp, "Result: %d", 42);

/* use result */

mem_allocators_temp_clear(temp);
```

The allocator determines the lifetime of all strings created through it.

This allows higher-level code to choose an appropriate memory strategy without requiring the string implementation to manage memory independently.

---

# C String Compatibility

Quasar strings are intended to interoperate with existing C APIs.

`str_cstr()` provides access to a null-terminated representation suitable for APIs expecting a standard C string.

`str_data()` provides direct access to the underlying string data.

The string API should be used when operating on `str_t` values.

Raw C string functions may still be used when interacting with APIs that require them.

Quasar does not prevent users from using standard C string functionality directly.

The purpose of `strings.h` is to provide a higher-level interface for common string operations while remaining compatible with normal C code.

---

# Byte Semantics

Quasar string positions, lengths, slices, and indexes are measured in bytes.

For example:

* `str_len()` returns the number of bytes.
* `str_slice()` operates on byte offsets.
* `str_substr()` operates on byte lengths.
* `str_index_of()` returns a byte index.
* `str_reverse()` operates on the underlying byte representation.

The base string API does not assume that strings contain a particular character encoding.

UTF-8-aware or other encoding-specific functionality may be provided separately where appropriate.

---

# Error and Boundary Behavior

String functions should handle invalid or out-of-range operations without causing undefined behavior.

Functions that operate on ranges should validate their input before accessing string data.

Functions that allocate memory should return an appropriate failure value if allocation fails.

For functions returning `str_t`, allocation failure should result in the designated invalid or empty string representation.

Functions that return indexes should use a consistent "not found" representation.

The exact sentinel used for "not found" should be defined by the public `strings.h` interface.

---

# Design Goals

The Quasar string system is designed around the following goals:

* Provide a high-level string interface for C.
* Eliminate repetitive manual string allocation and resizing.
* Make string ownership explicit.
* Integrate directly with Quasar allocators.
* Avoid requiring users to manually calculate string lengths for common operations.
* Provide convenient interoperability with C strings.
* Keep string operations independent of a specific allocation strategy.
* Provide common string functionality without requiring users to implement it themselves.
* Keep byte-oriented operations predictable and explicit.
* Allow higher-level Quasar APIs to build on a consistent string abstraction.

The string system is intended to be one of the primary high-level interfaces provided by Quasar.

The allocator system provides the memory management foundation, while `strings.h` provides a practical abstraction for one of the most common tasks performed in C programs.

Together, they allow code such as:

```c
Allocator *arena = mem_allocators_arena();

str_t name = str_from(arena, "Quasar");
str_t description = str_from(
 arena,
 "A higher-level standard library for C."
);

str_t message = str_fmt(
 arena,
 "%s: %s",
 str_cstr(name),
 str_cstr(description)
);

mem_allocator_free(arena);
```

The goal is not to replace C.

The goal is to make the parts of C that programmers repeatedly have to rebuild themselves feel like they should have existed in the first place.
