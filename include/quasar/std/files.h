#ifndef QUASAR_STD_FILES_H
#define QUASAR_STD_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <quasar/core/str.h>
#include <quasar/std/memory.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * files.h — Quasar standard filesystem and path API.
 *
 * The API is split into two logical namespaces:
 *
 *   path_* — pure string operations on paths (no I/O).
 *            These never touch the filesystem.
 *
 *   file_* — operations that interact with the filesystem.
 *
 * Both integrate with Quasar's existing Allocator* and str_t model.
 *
 * Path functions that return new strings take an Allocator* and
 * return a str_t owned by that allocator.
 *
 * Filesystem operations that return owned data (file contents,
 * directory listings, resolved paths, etc.) likewise take an
 * Allocator* and return memory owned by it.
 *
 * Boolean-returning operations indicate success or failure.
 * On failure the caller may inspect errno for details.
 * Quasar does not hide the underlying system error model.
 */

/*
 * ── Metadata types ─────────────────────────────────────────────────
 */

/*
 * FileInfo bundles common stat fields into a small value struct.
 * Use file_info() to fill it in one call instead of making
 * repeated stat queries.
 */
typedef struct {
	size_t size;
	time_t mtime, atime, ctime;
	mode_t mode;
	bool   is_file;
	bool   is_dir;
	bool   is_symlink;
} FileInfo;

/*
 * DirEntry represents a single entry in a directory listing.
 * The name field points into allocator-owned memory.
 */
typedef struct {
	str_t  name;
	bool   is_dir;
	bool   is_symlink;
	size_t size;
} DirEntry;

/*
 * FileWalkFn is the callback type for file_walk().
 * Return false from the callback to stop traversal early.
 */
typedef bool (*FileWalkFn)(str_t path, const FileInfo *info, void *userdata);

/*
 * ── Path operations  path_* ────────────────────────────────────────
 *
 * Path functions never touch the filesystem.  They operate only on
 * the string representation of a path and always return new str_t
 * values owned by the supplied allocator.
 */

/*
 * ── Joining and splitting ──────────────────────────────────────────
 */

/*
 * Append a single component to base, inserting a separator only
 * when required.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_join(Allocator *a, str_t base, str_t component);

/*
 * Join an array of path components with separators between them.
 *
 * Returns {NULL, 0} if a is NULL, parts is NULL, or allocation fails.
 */
str_t path_join_n(Allocator *a, str_t *parts, size_t count);

/*
 * ── Components ─────────────────────────────────────────────────────
 */

/*
 * Return the directory portion of path (everything before the
 * final separator).  Edge cases:
 *   "/foo"  → "/"
 *   "foo"   → "."
 *   "/"     → "/"
 *   "."     → "."
 *   ""      → "."
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_dirname(Allocator *a, str_t path);

/*
 * Return the final component of path (everything after the last
 * separator).  Edge cases:
 *   "/foo/bar" → "bar"
 *   "/foo/"    → "foo"
 *   "/"        → "/"
 *   ""         → "."
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_basename(Allocator *a, str_t path);

/*
 * Return the basename without its final extension.
 *   "/foo/bar.txt" → "bar"
 *   "/foo/bar"     → "bar"
 *   "/foo/.bar"    → ".bar"
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_stem(Allocator *a, str_t path);

/*
 * Return the extension of the final component (including the dot).
 *   "/foo/bar.txt" → ".txt"
 *   "/foo/bar"     → ""
 *   "/foo/.bar"    → ""
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_extension(Allocator *a, str_t path);

/*
 * Return the parent directory (one level up from path).
 * Equivalent to path_dirname() but the name signals intent.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_parent(Allocator *a, str_t path);

/*
 * ── Normalization and relativity ───────────────────────────────────
 */

/*
 * Clean "." and ".." components from path without resolving
 * symlinks.  Multiple slashes are collapsed to a single slash.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_normalize(Allocator *a, str_t path);

/*
 * Convert a relative path to absolute using the current working
 * directory.  If path is already absolute it is returned normalized.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_absolute(Allocator *a, str_t path);

/*
 * Compute a relative path from the directory 'from' to 'to'.
 * Both paths are resolved relative to the current directory
 * before the relative computation.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_relative(Allocator *a, str_t from, str_t to);

/*
 * Return true if path begins with a '/' (or platform root).
 */
bool path_is_absolute(str_t path);

/*
 * Return true if path does not begin with a root.
 */
bool path_is_relative(str_t path);

/*
 * ── Extension helpers ──────────────────────────────────────────────
 */

/*
 * Return true if path ends with the given extension.
 * The extension comparison is case-sensitive.
 */
bool path_has_extension(str_t path, str_t ext);

/*
 * Return path with its current extension replaced by new_ext.
 * If path has no extension, new_ext is appended.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_with_extension(Allocator *a, str_t path, str_t new_ext);

/*
 * Return path with its final extension removed.
 * If path has no extension it is returned unchanged.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t path_without_extension(Allocator *a, str_t path);

/*
 * ── Existence and type checks  file_* ──────────────────────────────
 *
 * All of these are thin, predictable wrappers.  They return false
 * on any error (including "does not exist"), so the common
 * "does this usable thing exist?" question is a single call.
 */

bool file_exists(str_t path);
bool file_is_file(str_t path);
bool file_is_dir(str_t path);
bool file_is_symlink(str_t path);
bool file_is_readable(str_t path);
bool file_is_writable(str_t path);
bool file_is_executable(str_t path);

/*
 * Return true if the file has zero size or the directory is empty.
 */
bool file_is_empty(str_t path);

/*
 * ── Metadata ───────────────────────────────────────────────────────
 */

/*
 * Individual metadata accessors.  Each returns a zero/sentinel
 * value on error (e.g. 0 for size, (time_t)-1 for times).
 */
size_t file_size(str_t path);
time_t file_mtime(str_t path);
time_t file_atime(str_t path);
time_t file_ctime(str_t path);
mode_t file_mode(str_t path);
uid_t  file_owner(str_t path);
gid_t  file_group(str_t path);

/*
 * Fill *out with file metadata in a single call.
 * Returns false on any error; *out is unchanged on failure.
 */
bool file_info(str_t path, FileInfo *out);

/*
 * ── Reading and writing ────────────────────────────────────────────
 */

/*
 * Read the entire contents of a file into a str_t.
 * The result is binary-safe (not null-termination-dependent)
 * and is owned by the supplied allocator.
 *
 * Returns {NULL, 0} if a is NULL or any error occurs.
 */
str_t file_read(Allocator *a, str_t path);

/*
 * Read the entire contents of a file as text.  The result is
 * guaranteed null-terminated for convenience with C string APIs.
 * Otherwise identical to file_read().
 *
 * Returns {NULL, 0} if a is NULL or any error occurs.
 */
str_t file_read_text(Allocator *a, str_t path);

/*
 * Write data to a file, overwriting any existing content.
 * Returns false on failure; errno is set.
 */
bool file_write(str_t path, const void *data, size_t len);

/*
 * Write a str_t to a file.  Convenience wrapper around file_write().
 */
bool file_write_str(str_t path, str_t content);

/*
 * Append data to the end of a file.  Creates the file if it does
 * not exist.  Returns false on failure; errno is set.
 */
bool file_append(str_t path, const void *data, size_t len);

/*
 * Append a str_t to a file.  Convenience wrapper around file_append().
 */
bool file_append_str(str_t path, str_t content);

/*
 * Read all lines from a file into an array of str_t.
 * *out_count receives the number of lines.
 * The array and its elements are owned by the allocator.
 *
 * Returns NULL on failure; *out_count is set to 0.
 */
str_t *file_read_lines(Allocator *a, str_t path, size_t *out_count);

/*
 * Write an array of str_t to a file, one per line.
 * Returns false on failure; errno is set.
 */
bool file_write_lines(str_t path, str_t *lines, size_t count);

/*
 * ── Directory operations ───────────────────────────────────────────
 */

/*
 * Create a single directory level.  Returns false on failure.
 */
bool file_mkdir(str_t path);

/*
 * Create directories recursively (like mkdir -p).
 * Returns false on failure.
 */
bool file_mkdir_p(str_t path);

/*
 * Remove an empty directory.  Returns false on failure.
 */
bool file_rmdir(str_t path);

/*
 * Remove a directory and all its contents recursively.
 * Returns false on failure.
 */
bool file_rmdir_r(str_t path);

/*
 * List the contents of a directory.  Returns an allocator-owned
 * array of DirEntry.  *out_count receives the number of entries.
 * "." and ".." are excluded.
 *
 * Returns NULL on failure; *out_count is set to 0.
 */
DirEntry *file_list(Allocator *a, str_t path, size_t *out_count);

/*
 * List a directory tree recursively (depth-first).
 * Otherwise identical to file_list().
 *
 * Returns NULL on failure; *out_count is set to 0.
 */
DirEntry *file_list_recursive(Allocator *a, str_t path, size_t *out_count);

/*
 * Return true if the directory exists and contains no entries
 * (other than "." and "..").
 */
bool file_dir_is_empty(str_t path);

/*
 * Return the number of entries in a directory (non-recursive,
 * excluding "." and "..").  Returns 0 on error.
 */
size_t file_dir_count(str_t path);

/*
 * ── Copy, move, and remove ─────────────────────────────────────────
 */

/*
 * Copy a single file from src to dst.  Returns false on failure.
 */
bool file_copy(str_t src, str_t dst);

/*
 * Copy a file or directory tree recursively from src to dst.
 * Returns false on failure.
 */
bool file_copy_r(str_t src, str_t dst);

/*
 * Move (rename) src to dst.  Falls back to copy + remove for
 * cross-device moves.  Returns false on failure.
 */
bool file_move(str_t src, str_t dst);

/*
 * Remove a file or empty directory.  Returns false on failure.
 */
bool file_remove(str_t path);

/*
 * Remove a file or directory tree recursively.
 * Returns false on failure.
 */
bool file_remove_r(str_t path);

/*
 * ── Symlinks ───────────────────────────────────────────────────────
 */

/*
 * Create a symbolic link at linkpath pointing to target.
 * Returns false on failure.
 */
bool file_symlink(str_t target, str_t linkpath);

/*
 * Read the target of a symbolic link.  The result is owned by
 * the allocator.
 *
 * Returns {NULL, 0} on failure.
 */
str_t file_readlink(Allocator *a, str_t path);

/*
 * Resolve all symlinks and produce a canonical absolute path.
 * Equivalent to realpath(3).  The result is owned by the allocator.
 *
 * Returns {NULL, 0} on failure.
 */
str_t file_realpath(Allocator *a, str_t path);

/*
 * ── Working directory and special paths ────────────────────────────
 */

/*
 * Return the current working directory.  Owned by the allocator.
 *
 * Returns {NULL, 0} on failure.
 */
str_t file_cwd(Allocator *a);

/*
 * Change the current working directory.  Returns false on failure.
 */
bool file_chdir(str_t path);

/*
 * Return the current user's home directory ($HOME).
 * Owned by the allocator.
 *
 * Returns {NULL, 0} on failure.
 */
str_t file_home(Allocator *a);

/*
 * Return the system temporary directory ($TMPDIR or /tmp).
 * Owned by the allocator.
 *
 * Returns {NULL, 0} on failure.
 */
str_t file_temp_dir(Allocator *a);

/*
 * Return the user-specific configuration directory
 * ($XDG_CONFIG_HOME or ~/.config).  Owned by the allocator.
 *
 * Returns {NULL, 0} on failure.
 */
str_t file_config_dir(Allocator *a);

/*
 * Return the user-specific data directory
 * ($XDG_DATA_HOME or ~/.local/share).  Owned by the allocator.
 *
 * Returns {NULL, 0} on failure.
 */
str_t file_data_dir(Allocator *a);

/*
 * ── Temporary files and directories ────────────────────────────────
 */

/*
 * Create a uniquely-named empty temporary file in the system
 * temp directory.  The path is owned by the allocator.
 * The caller is responsible for removing the file when done.
 *
 * Returns {NULL, 0} on failure.
 */
str_t file_temp_file(Allocator *a);

/*
 * Create a uniquely-named temporary directory in the system
 * temp directory.  The path is owned by the allocator.
 * The caller is responsible for removing the directory when done.
 *
 * Returns {NULL, 0} on failure.
 */
str_t file_temp_dir_create(Allocator *a);

/*
 * Remove a temporary file or directory if it resides under the
 * system temporary directory.  Returns false if the path is
 * outside the temp directory or removal fails.
 */
bool file_temp_cleanup(str_t path);

/*
 * ── Permissions and ownership ──────────────────────────────────────
 */

/*
 * Change file permissions.  Returns false on failure.
 */
bool file_chmod(str_t path, mode_t mode);

/*
 * Change file owner and group.  Returns false on failure.
 */
bool file_chown(str_t path, uid_t uid, gid_t gid);

/*
 * Change permissions recursively on a directory tree.
 * Returns false on failure.
 */
bool file_chmod_r(str_t path, mode_t mode);

/*
 * ── Convenience operations ─────────────────────────────────────────
 */

/*
 * Create an empty file if it does not exist.
 * If the file exists, update its modification time to now.
 * Returns false on failure.
 */
bool file_touch(str_t path);

/*
 * Write data to path atomically: write to a temporary file in
 * the same directory, then rename it over the target.
 * On failure the original file (if it existed) is unchanged.
 * Returns false on failure.
 */
bool file_write_atomic(str_t path, const void *data, size_t len);

/*
 * Walk a directory tree, calling fn for each entry.
 * Traversal is depth-first; directories are visited before
 * their contents.  Return false from fn to stop early.
 * Returns false if the initial root cannot be accessed.
 */
bool file_walk(str_t root, FileWalkFn fn, void *userdata);

#ifdef __cplusplus
}
#endif

#endif
