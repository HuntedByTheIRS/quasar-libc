#define _XOPEN_SOURCE 700
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../core/mem.h"
#include <quasar/core/fs.h>
#include <quasar/core/str.h>
#include <quasar/std/files.h>
#include <quasar/std/strings.h>

/*
 * Internal helper: duplicate a slice of a str_t into a new str_t.
 * Returns {NULL, 0} on allocation failure.
 */
static str_t _path_dup_slice(Allocator *a, str_t s, size_t start, size_t end)
{
	if (start > s.len) start = s.len;
	if (end > s.len) end = s.len;
	if (end <= start) return str_empty(a);
	return str_from_n(a, s.data + start, end - start);
}

/*
 * Internal: strip trailing '/' characters from a path.
 * Returns the adjusted length (index after last non-slash char, or 0).
 * Special case: a single "/" is left alone.
 */
static size_t _path_strip_trailing_slashes(str_t path)
{
	size_t end = path.len;
	if (end == 0) return 0;
	/* Never strip a lone root slash. */
	if (end == 1 && path.data[0] == '/') return 1;
	while (end > 1 && path.data[end - 1] == '/')
		end--;
	return end;
}

/*
 * Internal: find the last '/' in path.data[0..len), searching backward.
 * Returns the index of the '/' or STR_NPOS if not found.
 */
static size_t _path_find_last_slash(const char *data, size_t len)
{
	if (len == 0) return STR_NPOS;
	size_t i = len;
	while (i > 0) {
		i--;
		if (data[i] == '/') return i;
	}
	return STR_NPOS;
}

/* ====================================================================
 *  Path operations  path_*
 * ==================================================================== */

str_t
path_join(Allocator *a, str_t base, str_t component)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	/* Empty base → return copy of component. */
	if (base.len == 0)
		return str_dup(a, component);

	/* Empty component → return copy of base. */
	if (component.len == 0)
		return str_dup(a, base);

	/* Already has a separator at the join point. */
	bool need_sep = !(base.data[base.len - 1] == '/' ||
			  component.data[0] == '/');

	if (!need_sep)
		return str_cat(a, base, component);

	/* Insert a '/' separator. */
	str_t sep = {"/", 1};
	str_t tmp = str_cat(a, base, sep);
	if (!tmp.data) return empty;
	str_t result = str_cat(a, tmp, component);
	return result;
}

str_t
path_join_n(Allocator *a, str_t *parts, size_t count)
{
	str_t empty = {NULL, 0};
	if (!a || !parts || count == 0) {
		if (!a) return empty;
		return str_empty(a);
	}

	str_t result = str_dup(a, parts[0]);
	if (!result.data && parts[0].len > 0) return empty;

	for (size_t i = 1; i < count; i++) {
		result = path_join(a, result, parts[i]);
		if (!result.data) return empty;
	}
	return result;
}

/* ------------------------------------------------------------------ */
/*  Components                                                         */
/* ------------------------------------------------------------------ */

str_t
path_dirname(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	if (path.len == 0)
		return str_from(a, ".");

	/* Strip trailing slashes. */
	size_t end = _path_strip_trailing_slashes(path);

	/* Root "/" with trailing slashes stripped still yields 1. */
	if (end == 1 && path.data[0] == '/')
		return str_from(a, "/");

	size_t slash = _path_find_last_slash(path.data, end);

	if (slash == STR_NPOS)
		return str_from(a, ".");

	if (slash == 0)
		return str_from(a, "/");

	return _path_dup_slice(a, path, 0, slash);
}

str_t
path_basename(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	if (path.len == 0)
		return str_from(a, ".");

	/* "/" stays "/". */
	if (path.len == 1 && path.data[0] == '/')
		return str_from(a, "/");

	/* Strip trailing slashes. */
	size_t end = _path_strip_trailing_slashes(path);

	/* After stripping, if we only have "/", handle it. */
	if (end == 1 && path.data[0] == '/')
		return str_from(a, "/");

	size_t slash = _path_find_last_slash(path.data, end);

	if (slash == STR_NPOS)
		return _path_dup_slice(a, path, 0, end);

	return _path_dup_slice(a, path, slash + 1, end);
}

str_t
path_stem(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	str_t base = path_basename(a, path);
	if (!base.data) return empty;

	/* Find the last '.' that is not the first character. */
	size_t dot = STR_NPOS;
	size_t i = base.len;
	while (i > 1) { /* i > 1 ensures we skip position 0 */
		i--;
		if (base.data[i] == '.') {
			dot = i;
			break;
		}
	}

	if (dot == STR_NPOS)
		return base; /* Already allocated by path_basename */

	return _path_dup_slice(a, base, 0, dot);
}

str_t
path_extension(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	str_t base = path_basename(a, path);
	if (!base.data) return empty;

	/* Find the last '.' that is not the first character. */
	size_t i = base.len;
	while (i > 1) {
		i--;
		if (base.data[i] == '.')
			return _path_dup_slice(a, base, i, base.len);
	}

	return str_empty(a);
}

str_t
path_parent(Allocator *a, str_t path)
{
	return path_dirname(a, path);
}

/* ------------------------------------------------------------------ */
/*  Normalization and relativity                                       */
/* ------------------------------------------------------------------ */

/*
 * Internal: a single path component (pointer + length into original).
 */
typedef struct {
	const char *data;
	size_t      len;
} _path_comp_t;

str_t
path_normalize(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	if (path.len == 0)
		return str_from(a, ".");

	bool is_abs = (path.data[0] == '/');

	/*
	 * Allocate a working array of components.  Worst case: every
	 * other character is a '/', so max components = len/2 + 1.
	 */
	size_t max_comps = path.len / 2 + 1;
	_path_comp_t *comps = _quasar_mem_allocate(a,
				    max_comps * sizeof(_path_comp_t));
	if (!comps) return empty;

	size_t comp_count = 0;
	const char *p = path.data;
	const char *end = path.data + path.len;

	/* Parse components. */
	while (p < end) {
		/* Skip slashes. */
		while (p < end && *p == '/') p++;
		if (p >= end) break;

		/* Find end of this component. */
		const char *start = p;
		while (p < end && *p != '/') p++;

		size_t clen = (size_t)(p - start);

		/* Skip "." components. */
		if (clen == 1 && start[0] == '.')
			continue;

		/* ".." pops the previous component, or becomes a leading
		 * ".." for relative paths (matching POSIX/Python/Go behaviour). */
		if (clen == 2 && start[0] == '.' && start[1] == '.') {
			if (comp_count > 0) {
				comp_count--;
				continue;
			}
			if (!is_abs) {
				comps[comp_count].data = start;
				comps[comp_count].len  = clen;
				comp_count++;
			}
			continue;
		}

		comps[comp_count].data = start;
		comps[comp_count].len  = clen;
		comp_count++;
	}

	/* Build the result. */
	if (comp_count == 0) {
		if (is_abs)
			return str_from(a, "/");
		return str_from(a, ".");
	}

	/* Calculate total length needed. */
	size_t total = 0;
	if (is_abs) total = 1; /* leading '/' */
	for (size_t i = 0; i < comp_count; i++) {
		if (i > 0) total++; /* separator between components */
		total += comps[i].len;
	}

	char *buf = _quasar_mem_allocate(a, total + 1);
	if (!buf) return empty;

	char *wp = buf;
	if (is_abs) *wp++ = '/';
	for (size_t i = 0; i < comp_count; i++) {
		if (i > 0) *wp++ = '/';
		memcpy(wp, comps[i].data, comps[i].len);
		wp += comps[i].len;
	}
	*wp = '\0';

	return (str_t){buf, total};
}

str_t
path_absolute(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	if (path_is_absolute(path))
		return path_normalize(a, path);

	char *cwd_raw = _quasar_fs_cwd();
	if (!cwd_raw) return empty;

	str_t cwd = str_from(a, cwd_raw);
	free(cwd_raw);
	if (!cwd.data) return empty;

	str_t joined = path_join(a, cwd, path);
	if (!joined.data) return empty;

	return path_normalize(a, joined);
}

str_t
path_relative(Allocator *a, str_t from, str_t to)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	/*
	 * Resolve both paths relative to the current directory before
	 * computing the relative path, matching the documented contract.
	 */
	str_t nfrom = path_absolute(a, from);
	if (!nfrom.data) return empty;

	str_t nto = path_absolute(a, to);
	if (!nto.data) return empty;

	/* If both are identical, return ".". */
	if (str_equals(nfrom, nto))
		return str_from(a, ".");

	bool from_abs = (nfrom.data[0] == '/');
	bool to_abs   = (nto.data[0] == '/');

	/* Parse both into components (skip leading '/'). */
	size_t max_comps = (nfrom.len > nto.len ? nfrom.len : nto.len) / 2 + 1;
	_path_comp_t *from_comps = _quasar_mem_allocate(a,
				    max_comps * sizeof(_path_comp_t));
	_path_comp_t *to_comps   = _quasar_mem_allocate(a,
				    max_comps * sizeof(_path_comp_t));
	if (!from_comps || !to_comps) return empty;

	size_t from_cnt = 0, to_cnt = 0;

	/* Parse from. */
	{
		const char *p = nfrom.data + (from_abs ? 1 : 0);
		const char *e = nfrom.data + nfrom.len;
		while (p < e) {
			const char *s = p;
			while (p < e && *p != '/') p++;
			from_comps[from_cnt].data = s;
			from_comps[from_cnt].len  = (size_t)(p - s);
			from_cnt++;
			if (p < e) p++; /* skip '/' */
		}
	}

	/* Parse to. */
	{
		const char *p = nto.data + (to_abs ? 1 : 0);
		const char *e = nto.data + nto.len;
		while (p < e) {
			const char *s = p;
			while (p < e && *p != '/') p++;
			to_comps[to_cnt].data = s;
			to_comps[to_cnt].len  = (size_t)(p - s);
			to_cnt++;
			if (p < e) p++; /* skip '/' */
		}
	}

	/* Find common prefix length. */
	size_t common = 0;
	while (common < from_cnt && common < to_cnt &&
	       from_comps[common].len == to_comps[common].len &&
	       memcmp(from_comps[common].data, to_comps[common].data,
		      from_comps[common].len) == 0)
		common++;

	/*
	 * Build result: for each remaining from component, add "..".
	 * Then append remaining to components.
	 */
	size_t up_count = from_cnt - common;
	size_t rest_count = to_cnt - common;
	size_t total_comps = up_count + rest_count;

	if (total_comps == 0)
		return str_from(a, ".");

	str_t *parts = _quasar_mem_allocate(a, total_comps * sizeof(str_t));
	if (!parts) return empty;

	str_t dotdot = {"..", 2};

	for (size_t i = 0; i < up_count; i++)
		parts[i] = dotdot;

	for (size_t i = 0; i < rest_count; i++) {
		str_t comp = str_from_n(a, to_comps[common + i].data,
					 to_comps[common + i].len);
		if (!comp.data) return empty;
		parts[up_count + i] = comp;
	}

	str_t result = str_join(a, parts, total_comps, (str_t){"/", 1});
	return result;
}

/* ------------------------------------------------------------------ */
/*  Predicates                                                         */
/* ------------------------------------------------------------------ */

bool
path_is_absolute(str_t path)
{
	return path.len > 0 && path.data[0] == '/';
}

bool
path_is_relative(str_t path)
{
	return !path_is_absolute(path);
}

/* ------------------------------------------------------------------ */
/*  Extension helpers                                                  */
/* ------------------------------------------------------------------ */

bool
path_has_extension(str_t path, str_t ext)
{
	/*
	 * We need the extension without allocating.  Compute it
	 * in-place by finding the basename and its dot.
	 */
	if (path.len == 0) return ext.len == 0;

	/* Strip trailing slashes. */
	size_t end = _path_strip_trailing_slashes(path);
	if (end == 0) return ext.len == 0;

	/* Find basename start. */
	size_t slash = _path_find_last_slash(path.data, end);
	size_t base_start = (slash == STR_NPOS) ? 0 : slash + 1;
	size_t base_len = end - base_start;

	/* Find last '.' in basename (not first char). */
	size_t dot = STR_NPOS;
	size_t i = base_start + base_len;
	while (i > base_start + 1) {
		i--;
		if (path.data[i] == '.') {
			dot = i;
			break;
		}
	}

	if (ext.len == 0)
		return (dot == STR_NPOS);

	if (dot == STR_NPOS)
		return false;

	size_t ext_start = dot;
	size_t ext_len = end - ext_start;

	if (ext_len != ext.len)
		return false;

	return memcmp(path.data + ext_start, ext.data, ext.len) == 0;
}

str_t
path_with_extension(Allocator *a, str_t path, str_t new_ext)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	str_t without = path_without_extension(a, path);
	if (!without.data) return empty;

	return str_cat(a, without, new_ext);
}

str_t
path_without_extension(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	if (path.len == 0)
		return str_empty(a);

	/* Strip trailing slashes. */
	size_t end = _path_strip_trailing_slashes(path);

	/* Find basename start. */
	size_t slash = _path_find_last_slash(path.data, end);
	size_t base_start = (slash == STR_NPOS) ? 0 : slash + 1;

	/* Find last '.' in basename (not first char of basename). */
	size_t dot = STR_NPOS;
	size_t i = end;
	while (i > base_start + 1) {
		i--;
		if (path.data[i] == '.') {
			dot = i;
			break;
		}
	}

	/* No extension found — return a copy of the original. */
	if (dot == STR_NPOS)
		return str_dup(a, path);

	/*
	 * Build result: everything before the dot.
	 * This includes the dirname + "/" + stem.
	 */
	return _path_dup_slice(a, path, 0, dot);
}

/* ====================================================================
 *  File operations  file_*
 * ==================================================================== */

/* ------------------------------------------------------------------ */
/*  Existence and type checks                                          */
/* ------------------------------------------------------------------ */

bool
file_exists(str_t path)
{
	return _quasar_fs_exists(str_cstr(path));
}

bool
file_is_file(str_t path)
{
	return _quasar_fs_is_file(str_cstr(path));
}

bool
file_is_dir(str_t path)
{
	return _quasar_fs_is_dir(str_cstr(path));
}

bool
file_is_symlink(str_t path)
{
	return _quasar_fs_is_symlink(str_cstr(path));
}

bool
file_is_readable(str_t path)
{
	return _quasar_fs_is_readable(str_cstr(path));
}

bool
file_is_writable(str_t path)
{
	return _quasar_fs_is_writable(str_cstr(path));
}

bool
file_is_executable(str_t path)
{
	return _quasar_fs_is_executable(str_cstr(path));
}

bool
file_is_empty(str_t path)
{
	QsFileStat st;

	if (!_quasar_fs_stat(str_cstr(path), &st))
		return false;

	if (st.is_dir) {
		size_t count = 0;
		QsDirEntry *entries = _quasar_fs_list(str_cstr(path), &count);
		if (!entries) return false;
		_quasar_fs_list_free(entries, count);
		return count == 0;
	}

	return st.size == 0;
}

/* ------------------------------------------------------------------ */
/*  Metadata                                                           */
/* ------------------------------------------------------------------ */

size_t
file_size(str_t path)
{
	QsFileStat st;
	if (!_quasar_fs_stat(str_cstr(path), &st))
		return 0;
	return st.size;
}

time_t
file_mtime(str_t path)
{
	QsFileStat st;
	if (!_quasar_fs_stat(str_cstr(path), &st))
		return (time_t)-1;
	return st.mtime;
}

time_t
file_atime(str_t path)
{
	QsFileStat st;
	if (!_quasar_fs_stat(str_cstr(path), &st))
		return (time_t)-1;
	return st.atime;
}

time_t
file_ctime(str_t path)
{
	QsFileStat st;
	if (!_quasar_fs_stat(str_cstr(path), &st))
		return (time_t)-1;
	return st.ctime;
}

mode_t
file_mode(str_t path)
{
	QsFileStat st;
	if (!_quasar_fs_stat(str_cstr(path), &st))
		return 0;
	return st.mode;
}

uid_t
file_owner(str_t path)
{
	QsFileStat st;
	if (!_quasar_fs_stat(str_cstr(path), &st))
		return (uid_t)-1;
	return st.uid;
}

gid_t
file_group(str_t path)
{
	QsFileStat st;
	if (!_quasar_fs_stat(str_cstr(path), &st))
		return (gid_t)-1;
	return st.gid;
}

bool
file_info(str_t path, FileInfo *out)
{
	QsFileStat st;

	if (!out) return false;
	if (!_quasar_fs_stat(str_cstr(path), &st))
		return false;

	out->size      = st.size;
	out->mtime     = st.mtime;
	out->atime     = st.atime;
	out->ctime     = st.ctime;
	out->mode      = st.mode;
	out->is_file   = st.is_file;
	out->is_dir    = st.is_dir;
	out->is_symlink = st.is_symlink;
	return true;
}

/* ------------------------------------------------------------------ */
/*  Reading and writing                                                */
/* ------------------------------------------------------------------ */

str_t
file_read(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	size_t len = 0;
	char *data = _quasar_fs_read(str_cstr(path), &len);
	/*
	 * _quasar_fs_read signals errors with *out_len == (size_t)-1
	 * and returns NULL.  Empty files return NULL with *out_len == 0.
	 */
	if (!data) {
		if (len == 0) return str_empty(a); /* empty file */
		return empty;                        /* error */
	}

	str_t result = str_from_n(a, data, len);
	free(data);
	return result;
}

str_t
file_read_text(Allocator *a, str_t path)
{
	return file_read(a, path);
}

bool
file_write(str_t path, const void *data, size_t len)
{
	return _quasar_fs_write(str_cstr(path), data, len);
}

bool
file_write_str(str_t path, str_t content)
{
	return file_write(path, content.data, content.len);
}

bool
file_append(str_t path, const void *data, size_t len)
{
	return _quasar_fs_append(str_cstr(path), data, len);
}

bool
file_append_str(str_t path, str_t content)
{
	return file_append(path, content.data, content.len);
}

str_t *
file_read_lines(Allocator *a, str_t path, size_t *out_count)
{
	if (out_count) *out_count = 0;
	if (!a) return NULL;

	str_t content = file_read_text(a, path);
	if (!content.data) return NULL; /* error */

	if (content.len == 0) {
		/* Empty file: return non-NULL sentinel so callers
		 * can distinguish empty from error. */
		str_t *lines = _quasar_mem_allocate(a, sizeof(str_t));
		if (!lines) return NULL;
		return lines;
	}

	/* Split by '\n'. */
	str_t delim = {"\n", 1};
	size_t count = 0;
	str_t *lines = str_split(a, content, delim, &count);
	if (!lines) return NULL;

	/*
	 * If the file ends with a newline, str_split gives us a trailing
	 * empty string.  We need to remove it to match expected behavior:
	 * "a\nb\n" yields 2 lines, not 3.
	 * The trailing empty element has .data pointing to the original
	 * string's null terminator with .len == 0.  We just decrement
	 * count to skip it.
	 */
	if (count > 0 && lines[count - 1].len == 0)
		count--;

	if (out_count) *out_count = count;
	return lines;
}

bool
file_write_lines(str_t path, str_t *lines, size_t count)
{
	if (count == 0)
		return file_write(path, "", 0);

	/*
	 * We need to join with '\n' and add a trailing newline.
	 * Create a temp arena for the intermediate string.
	 */
	Allocator *tmp = mem_allocators_arena();
	if (!tmp) return false;

	str_t delim = {"\n", 1};
	str_t joined = str_join(tmp, lines, count, delim);
	if (!joined.data) {
		mem_allocator_free(tmp);
		return false;
	}

	/* Add trailing newline. */
	str_t nl = {"\n", 1};
	str_t with_nl = str_cat(tmp, joined, nl);
	if (!with_nl.data) {
		mem_allocator_free(tmp);
		return false;
	}

	bool ok = file_write_str(path, with_nl);
	mem_allocator_free(tmp);
	return ok;
}

/* ------------------------------------------------------------------ */
/*  Directory operations                                               */
/* ------------------------------------------------------------------ */

bool
file_mkdir(str_t path)
{
	return _quasar_fs_mkdir(str_cstr(path));
}

bool
file_mkdir_p(str_t path)
{
	return _quasar_fs_mkdir_p(str_cstr(path));
}

bool
file_rmdir(str_t path)
{
	return _quasar_fs_rmdir(str_cstr(path));
}

bool
file_rmdir_r(str_t path)
{
	return _quasar_fs_rmdir_r(str_cstr(path));
}

DirEntry *
file_list(Allocator *a, str_t path, size_t *out_count)
{
	if (out_count) *out_count = 0;
	if (!a) return NULL;

	size_t count = 0;
	QsDirEntry *raw = _quasar_fs_list(str_cstr(path), &count);
	if (!raw) return NULL;

	/*
	 * For empty directories, return a non-NULL sentinel so callers
	 * can distinguish "empty" (valid pointer, *out_count=0) from
	 * "error" (NULL).
	 */
	size_t alloc_count = (count > 0) ? count : 1;
	DirEntry *entries = _quasar_mem_allocate(a,
				    alloc_count * sizeof(DirEntry));
	if (!entries) {
		_quasar_fs_list_free(raw, count);
		return NULL;
	}

	for (size_t i = 0; i < count; i++) {
		str_t name = str_from(a, raw[i].name);
		if (!name.data) {
			_quasar_fs_list_free(raw, count);
			return NULL;
		}
		entries[i].name      = name;
		entries[i].is_dir    = raw[i].is_dir;
		entries[i].is_symlink = raw[i].is_symlink;
		entries[i].size      = raw[i].size;
	}

	_quasar_fs_list_free(raw, count);
	if (out_count) *out_count = count;
	return entries;
}

DirEntry *
file_list_recursive(Allocator *a, str_t path, size_t *out_count)
{
	if (out_count) *out_count = 0;
	if (!a) return NULL;

	size_t count = 0;
	QsDirEntry *raw = _quasar_fs_list_recursive(str_cstr(path), &count);
	if (!raw) return NULL;

	/*
	 * For empty directories, return a non-NULL sentinel so callers
	 * can distinguish "empty" (valid pointer, *out_count=0) from
	 * "error" (NULL).
	 */
	size_t alloc_count = (count > 0) ? count : 1;
	DirEntry *entries = _quasar_mem_allocate(a,
				    alloc_count * sizeof(DirEntry));
	if (!entries) {
		_quasar_fs_list_free(raw, count);
		return NULL;
	}

	for (size_t i = 0; i < count; i++) {
		str_t name = str_from(a, raw[i].name);
		if (!name.data) {
			_quasar_fs_list_free(raw, count);
			return NULL;
		}
		entries[i].name      = name;
		entries[i].is_dir    = raw[i].is_dir;
		entries[i].is_symlink = raw[i].is_symlink;
		entries[i].size      = raw[i].size;
	}

	_quasar_fs_list_free(raw, count);
	if (out_count) *out_count = count;
	return entries;
}

bool
file_dir_is_empty(str_t path)
{
	size_t count = 0;
	QsDirEntry *entries = _quasar_fs_list(str_cstr(path), &count);
	if (!entries) return false;
	_quasar_fs_list_free(entries, count);
	return count == 0;
}

size_t
file_dir_count(str_t path)
{
	size_t count = 0;
	QsDirEntry *entries = _quasar_fs_list(str_cstr(path), &count);
	if (!entries) return 0;
	_quasar_fs_list_free(entries, count);
	return count;
}

/* ------------------------------------------------------------------ */
/*  Copy, move, and remove                                             */
/* ------------------------------------------------------------------ */

bool
file_copy(str_t src, str_t dst)
{
	return _quasar_fs_copy(str_cstr(src), str_cstr(dst));
}

bool
file_copy_r(str_t src, str_t dst)
{
	return _quasar_fs_copy_r(str_cstr(src), str_cstr(dst));
}

bool
file_move(str_t src, str_t dst)
{
	return _quasar_fs_move(str_cstr(src), str_cstr(dst));
}

bool
file_remove(str_t path)
{
	return _quasar_fs_remove(str_cstr(path));
}

bool
file_remove_r(str_t path)
{
	return _quasar_fs_remove_r(str_cstr(path));
}

/* ------------------------------------------------------------------ */
/*  Symlinks                                                           */
/* ------------------------------------------------------------------ */

bool
file_symlink(str_t target, str_t linkpath)
{
	return _quasar_fs_symlink(str_cstr(target), str_cstr(linkpath));
}

str_t
file_readlink(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	char *raw = _quasar_fs_readlink(str_cstr(path));
	if (!raw) return empty;

	str_t result = str_from(a, raw);
	free(raw);
	return result;
}

str_t
file_realpath(Allocator *a, str_t path)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	char *raw = _quasar_fs_realpath(str_cstr(path));
	if (!raw) return empty;

	str_t result = str_from(a, raw);
	free(raw);
	return result;
}

/* ------------------------------------------------------------------ */
/*  Working directory and special paths                                */
/* ------------------------------------------------------------------ */

str_t
file_cwd(Allocator *a)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	char *raw = _quasar_fs_cwd();
	if (!raw) return empty;

	str_t result = str_from(a, raw);
	free(raw);
	return result;
}

bool
file_chdir(str_t path)
{
	return _quasar_fs_chdir(str_cstr(path));
}

str_t
file_home(Allocator *a)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	char *raw = _quasar_fs_home();
	if (!raw) return empty;

	str_t result = str_from(a, raw);
	free(raw);
	return result;
}

str_t
file_temp_dir(Allocator *a)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	char *raw = _quasar_fs_temp_dir();
	if (!raw) return empty;

	str_t result = str_from(a, raw);
	free(raw);
	return result;
}

str_t
file_config_dir(Allocator *a)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	char *raw = _quasar_fs_config_dir();
	if (!raw) return empty;

	str_t result = str_from(a, raw);
	free(raw);
	return result;
}

str_t
file_data_dir(Allocator *a)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	char *raw = _quasar_fs_data_dir();
	if (!raw) return empty;

	str_t result = str_from(a, raw);
	free(raw);
	return result;
}

/* ------------------------------------------------------------------ */
/*  Temporary files and directories                                    */
/* ------------------------------------------------------------------ */

str_t
file_temp_file(Allocator *a)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	char *tmpdir = _quasar_fs_temp_dir();
	if (!tmpdir) return empty;

	/* Build template: <tmpdir>/quasar-XXXXXX */
	size_t tmplen = strlen(tmpdir) + 1 + strlen("quasar-XXXXXX") + 1;
	char *tmpl = _quasar_mem_allocate(a, tmplen);
	if (!tmpl) {
		free(tmpdir);
		return empty;
	}

	snprintf(tmpl, tmplen, "%s/quasar-XXXXXX", tmpdir);
	free(tmpdir);

	int fd = mkstemp(tmpl);
	if (fd < 0) return empty;
	close(fd);

	return str_from(a, tmpl);
}

str_t
file_temp_dir_create(Allocator *a)
{
	str_t empty = {NULL, 0};
	if (!a) return empty;

	char *tmpdir = _quasar_fs_temp_dir();
	if (!tmpdir) return empty;

	size_t tmplen = strlen(tmpdir) + 1 + strlen("quasar-dir-XXXXXX") + 1;
	char *tmpl = _quasar_mem_allocate(a, tmplen);
	if (!tmpl) {
		free(tmpdir);
		return empty;
	}

	snprintf(tmpl, tmplen, "%s/quasar-dir-XXXXXX", tmpdir);
	free(tmpdir);

	if (!mkdtemp(tmpl)) return empty;

	return str_from(a, tmpl);
}

bool
file_temp_cleanup(str_t path)
{
	/* Get the temp directory. */
	char *tmpdir_raw = _quasar_fs_temp_dir();
	if (!tmpdir_raw) return false;

	size_t tmpdir_len = strlen(tmpdir_raw);

	/*
	 * Check if path starts with the temp directory followed by
	 * a separator or end.
	 */
	bool in_temp = false;
	if (path.len >= tmpdir_len &&
	    memcmp(path.data, tmpdir_raw, tmpdir_len) == 0) {
		if (path.len == tmpdir_len ||
		    path.data[tmpdir_len] == '/')
			in_temp = true;
	}

	free(tmpdir_raw);

	if (!in_temp) return false;

	return _quasar_fs_remove_r(str_cstr(path));
}

/* ------------------------------------------------------------------ */
/*  Permissions and ownership                                          */
/* ------------------------------------------------------------------ */

bool
file_chmod(str_t path, mode_t mode)
{
	return _quasar_fs_chmod(str_cstr(path), mode);
}

bool
file_chown(str_t path, uid_t uid, gid_t gid)
{
	return _quasar_fs_chown(str_cstr(path), uid, gid);
}

struct _chmod_r_ctx {
	mode_t mode;
	bool   ok;
};

static bool _chmod_r_cb(str_t path, const FileInfo *info, void *userdata)
{
	struct _chmod_r_ctx *ctx = userdata;
	(void)info;
	if (!_quasar_fs_chmod(str_cstr(path), ctx->mode))
		ctx->ok = false;
	return ctx->ok;
}

bool
file_chmod_r(str_t path, mode_t mode)
{
	/*
	 * Stat first to determine type, then chmod the root itself.
	 * If it's a directory, walk the tree.
	 */
	QsFileStat st;
	if (!_quasar_fs_stat(str_cstr(path), &st))
		return false;

	if (!_quasar_fs_chmod(str_cstr(path), mode))
		return false;

	if (!st.is_dir)
		return true;

	struct _chmod_r_ctx ctx = {mode, true};
	file_walk(path, _chmod_r_cb, &ctx);
	return ctx.ok;
}

/* ------------------------------------------------------------------ */
/*  Convenience operations                                             */
/* ------------------------------------------------------------------ */

bool
file_touch(str_t path)
{
	return _quasar_fs_touch(str_cstr(path));
}

bool
file_write_atomic(str_t path, const void *data, size_t len)
{
	return _quasar_fs_write_atomic(str_cstr(path), data, len);
}

/* ------------------------------------------------------------------ */
/*  Walk                                                               */
/* ------------------------------------------------------------------ */

/*
 * Cycle-detection: track (dev, ino) of visited directories to
 * prevent infinite loops from symlink cycles.
 */
typedef struct {
	dev_t dev;
	ino_t ino;
} _visited_ino;

typedef struct {
	_visited_ino *entries;
	size_t        count;
	size_t        cap;
} _visited_set;

static bool _visited_init(_visited_set *vs, Allocator *a)
{
	vs->cap  = 64;
	vs->count = 0;
	vs->entries = _quasar_mem_allocate(a, vs->cap * sizeof(_visited_ino));
	return vs->entries != NULL;
}

static bool _visited_has(_visited_set *vs, dev_t dev, ino_t ino)
{
	for (size_t i = 0; i < vs->count; i++)
		if (vs->entries[i].dev == dev && vs->entries[i].ino == ino)
			return true;
	return false;
}

static bool _visited_add(_visited_set *vs, Allocator *a,
			 dev_t dev, ino_t ino)
{
	if (vs->count >= vs->cap) {
		size_t new_cap = vs->cap * 2;
		_visited_ino *new_entries;
		new_entries = _quasar_mem_allocate(a,
				      new_cap * sizeof(_visited_ino));
		if (!new_entries) return false;
		for (size_t i = 0; i < vs->count; i++)
			new_entries[i] = vs->entries[i];
		/*
		 * Old entries buffer is abandoned in the arena
		 * (file_walk always uses an arena allocator).
		 * Individual arena allocations cannot be freed.
		 */
		vs->entries = new_entries;
		vs->cap     = new_cap;
	}
	vs->entries[vs->count].dev = dev;
	vs->entries[vs->count].ino = ino;
	vs->count++;
	return true;
}

/*
 * Internal recursive walk helper.  a is a temporary allocator
 * that owns all intermediate strings and listings.
 */
static bool _file_walk_internal(Allocator *a, _visited_set *vs,
				str_t root, FileWalkFn fn, void *userdata)
{
	QsFileStat st;
	FileInfo info;

	if (!_quasar_fs_lstat(str_cstr(root), &st)) return false;
	info.size     = st.size;
	info.mtime    = st.mtime;
	info.atime    = st.atime;
	info.ctime    = st.ctime;
	info.mode     = st.mode;
	info.is_file  = st.is_file;
	info.is_dir   = st.is_dir;
	info.is_symlink = st.is_symlink;

	/* Visit the current entry. */
	if (!fn(root, &info, userdata)) return true; /* caller stopped */

	if (!info.is_dir) return true;

	/*
	 * Cycle detection: check if we've already visited this
	 * directory's (device, inode) pair.  If so, we've hit
	 * a symlink cycle — stop descending.
	 */
	if (_visited_has(vs, st.dev, st.ino)) return true;
	if (!_visited_add(vs, a, st.dev, st.ino)) return false;

	/* List and recurse into children. */
	size_t count = 0;
	DirEntry *entries = file_list(a, root, &count);
	if (!entries) return false;

	for (size_t i = 0; i < count; i++) {
		str_t child = path_join(a, root, entries[i].name);
		if (!child.data) return false;
		if (!_file_walk_internal(a, vs, child, fn, userdata))
			return false;
	}

	return true;
}

bool
file_walk(str_t root, FileWalkFn fn, void *userdata)
{
	if (!fn) return false;

	/*
	 * Create a temporary arena for intermediate strings and
	 * directory listings used during the walk.
	 */
	Allocator *arena = mem_allocators_arena();
	if (!arena) return false;

	_visited_set vs;
	bool ok = false;
	if (!_visited_init(&vs, arena)) goto done;
	ok = _file_walk_internal(arena, &vs, root, fn, userdata);
done:
	mem_allocator_free(arena);
	return ok;
}
