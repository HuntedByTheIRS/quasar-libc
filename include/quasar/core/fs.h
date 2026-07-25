#ifndef QUASAR_CORE_FS_H
#define QUASAR_CORE_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * core/fs.h — Quasar internal filesystem primitives.
 *
 * These are thin, predictable wrappers around POSIX filesystem calls.
 * They operate on C strings (const char *) and raw POSIX types —
 * they do NOT depend on Allocator* or str_t.
 *
 * Functions that return allocated memory (read, readlink, cwd, etc.)
 * use malloc() / free().  The std layer wraps these with Allocator*
 * and str_t for the public API.
 *
 * Boolean-returning functions return true on success, false on failure.
 * On failure, errno is set by the underlying POSIX call.
 */

/*
 * Internal stat result.  Mirrors the public FileInfo but uses only
 * raw POSIX types and simple bools.
 */
typedef struct {
	size_t size;
	time_t mtime, atime, ctime;
	mode_t mode;
	uid_t  uid;
	gid_t  gid;
	dev_t  dev;
	ino_t  ino;
	bool   is_file;
	bool   is_dir;
	bool   is_symlink;
} QsFileStat;

/*
 * Internal directory entry.  name is malloc'd and must be freed
 * via _quasar_fs_list_free().  The caller owns the entire array
 * and must free it with _quasar_fs_list_free(entries, count).
 */
typedef struct {
	char  *name;
	bool   is_dir;
	bool   is_symlink;
	size_t size;
} QsDirEntry;

/*
 * ── Stat operations ────────────────────────────────────────────────
 *
 * _quasar_fs_stat  wraps stat(2)  — follows symlinks.
 * _quasar_fs_lstat wraps lstat(2) — does not follow symlinks.
 *
 * Return false (and leave *out unchanged) on any error.
 */
bool _quasar_fs_stat(const char *path, QsFileStat *out);
bool _quasar_fs_lstat(const char *path, QsFileStat *out);

/*
 * ── Existence and type checks ──────────────────────────────────────
 *
 * Each returns false on any error (including ENOENT), so the common
 * "does this usable thing exist?" question is a single call.
 */
bool _quasar_fs_exists(const char *path);
bool _quasar_fs_is_file(const char *path);
bool _quasar_fs_is_dir(const char *path);
bool _quasar_fs_is_symlink(const char *path);
bool _quasar_fs_is_readable(const char *path);
bool _quasar_fs_is_writable(const char *path);
bool _quasar_fs_is_executable(const char *path);

/*
 * ── File I/O ───────────────────────────────────────────────────────
 *
 * _quasar_fs_read reads the entire file into a malloc'd buffer.
 *   *out_len receives the number of bytes read on success,
 *   (size_t)-1 on error, or 0 for an empty file.
 *   Returns NULL on failure or empty file; the caller
 *   distinguishes error from empty by inspecting *out_len.
 *   The caller must free() the returned buffer on success.
 *
 * _quasar_fs_write overwrites an existing file (or creates a new one).
 *   Returns false on failure; errno is set.
 *
 * _quasar_fs_append appends data to the end of a file.
 *   Creates the file if it does not exist.
 *   Returns false on failure; errno is set.
 */
char *_quasar_fs_read(const char *path, size_t *out_len);
bool  _quasar_fs_write(const char *path, const void *data, size_t len);
bool  _quasar_fs_append(const char *path, const void *data, size_t len);

/*
 * ── Directory creation ─────────────────────────────────────────────
 *
 * _quasar_fs_mkdir    creates a single directory level (like mkdir(2)).
 * _quasar_fs_mkdir_p  creates directories recursively (like mkdir -p).
 * _quasar_fs_rmdir    removes an empty directory only.
 * _quasar_fs_rmdir_r  removes a directory and all its contents recursively.
 */
bool _quasar_fs_mkdir(const char *path);
bool _quasar_fs_mkdir_p(const char *path);
bool _quasar_fs_rmdir(const char *path);
bool _quasar_fs_rmdir_r(const char *path);

/*
 * ── Directory listing ──────────────────────────────────────────────
 *
 * _quasar_fs_list returns a malloc'd array of QsDirEntry.
 *   *out_count receives the number of entries.
 *   The caller must free the array via _quasar_fs_list_free().
 *   "." and ".." are excluded from the listing.
 *
 * _quasar_fs_list_recursive walks the tree depth-first.
 *   Otherwise identical to _quasar_fs_list.
 */
QsDirEntry *_quasar_fs_list(const char *path, size_t *out_count);
QsDirEntry *_quasar_fs_list_recursive(const char *path, size_t *out_count);
void        _quasar_fs_list_free(QsDirEntry *entries, size_t count);

/*
 * ── Copy, move, and remove ─────────────────────────────────────────
 *
 * _quasar_fs_copy    copies a single file.
 * _quasar_fs_copy_r  copies a file or directory tree recursively.
 * _quasar_fs_move    renames; falls back to copy+remove for cross-device.
 * _quasar_fs_remove  removes a file or empty directory.
 * _quasar_fs_remove_r removes a file or directory tree recursively.
 */
bool _quasar_fs_copy(const char *src, const char *dst);
bool _quasar_fs_copy_r(const char *src, const char *dst);
bool _quasar_fs_move(const char *src, const char *dst);
bool _quasar_fs_remove(const char *path);
bool _quasar_fs_remove_r(const char *path);

/*
 * ── Symlinks ───────────────────────────────────────────────────────
 *
 * _quasar_fs_symlink   creates a symbolic link.
 * _quasar_fs_readlink  reads the target of a symlink (malloc'd, caller frees).
 * _quasar_fs_realpath  resolves all symlinks, produces canonical absolute
 *                       path (malloc'd, caller frees).
 */
bool  _quasar_fs_symlink(const char *target, const char *linkpath);
char *_quasar_fs_readlink(const char *path);
char *_quasar_fs_realpath(const char *path);

/*
 * ── Working directory ──────────────────────────────────────────────
 *
 * _quasar_fs_cwd    returns the current working directory (malloc'd).
 * _quasar_fs_chdir  changes the current working directory.
 */
char *_quasar_fs_cwd(void);
bool  _quasar_fs_chdir(const char *path);

/*
 * ── Special paths ──────────────────────────────────────────────────
 *
 * Each returns a malloc'd string that the caller must free().
 * _quasar_fs_config_dir respects XDG_CONFIG_HOME.
 * _quasar_fs_data_dir   respects XDG_DATA_HOME.
 */
char *_quasar_fs_home(void);
char *_quasar_fs_temp_dir(void);
char *_quasar_fs_config_dir(void);
char *_quasar_fs_data_dir(void);

/*
 * ── Permissions ────────────────────────────────────────────────────
 */
bool _quasar_fs_chmod(const char *path, mode_t mode);
bool _quasar_fs_chown(const char *path, uid_t uid, gid_t gid);

/*
 * ── Convenience ────────────────────────────────────────────────────
 *
 * _quasar_fs_touch creates an empty file (if missing) and updates
 *   its modification time.
 *
 * _quasar_fs_write_atomic writes to a temporary file in the same
 *   directory, then renames it over the target.  On failure the
 *   target (if it existed) is left unchanged.
 */
bool _quasar_fs_touch(const char *path);
bool _quasar_fs_write_atomic(const char *path, const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
