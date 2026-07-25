```markdown
# std/files.h

<!--toc:start-->
- [std/files.h](#stdfilesh)
  - [Ownership](#ownership)
  - [Path operations `path_*`](#path-operations-path)
    - [Joining and splitting](#joining-and-splitting)
    - [Components](#components)
    - [Normalization and relativity](#normalization-and-relativity)
    - [Extension helpers](#extension-helpers)
  - [Existence and type checks `file_*`](#existence-and-type-checks-file)
  - [Metadata](#metadata)
  - [Reading and writing](#reading-and-writing)
  - [Directory operations](#directory-operations)
  - [Copy, move, and remove](#copy-move-and-remove)
  - [Symlinks](#symlinks)
  - [Working directory and special paths](#working-directory-and-special-paths)
  - [Temporary files and directories](#temporary-files-and-directories)
  - [Permissions and ownership](#permissions-and-ownership)
  - [Convenience operations](#convenience-operations)
  - [Compatibility with libc](#compatibility-with-libc)
  - [Design goals](#design-goals)
<!--toc:end-->

`files.h` is part of the Quasar standard library. It provides practical, allocator-aware helpers for path manipulation and filesystem operations so that programs do not need to repeatedly re-implement the same thin wrappers around POSIX calls.

The API is split into two logical namespaces:

- `path_*` — pure string operations on paths (no I/O)
- `file_*` — operations that interact with the filesystem

Both integrate with Quasar’s existing `Allocator*` and `str_t` model.

---

## Ownership

Path functions that return new strings take an `Allocator*` and return a `str_t` owned by that allocator.

Filesystem operations that return owned data (file contents, directory listings, resolved paths, etc.) likewise take an `Allocator*` and return memory owned by it.

Boolean-returning operations indicate success or failure. On failure the caller may inspect `errno` for details. Quasar does not hide the underlying system error model.

Objects returned by these functions do not own the allocator that created them. Their lifetime is governed by the allocator’s rules (see `std/memory.h`).

---

## Path operations `path_*`

Path functions never touch the filesystem. They operate only on the string representation of a path and always return new `str_t` values owned by the supplied allocator.

### Joining and splitting

```c
str_t path_join(Allocator *a, str_t base, str_t component);
str_t path_join_n(Allocator *a, str_t *parts, size_t count);
```

`path_join` appends a single component, inserting a separator only when required.  
`path_join_n` joins an array of components.

### Components

```c
str_t path_dirname(Allocator *a, str_t path);
str_t path_basename(Allocator *a, str_t path);
str_t path_stem(Allocator *a, str_t path);       /* basename without extension */
str_t path_extension(Allocator *a, str_t path);
str_t path_parent(Allocator *a, str_t path);     /* one directory level up */
```

These extract the conventional path components. Empty or root-edge cases produce sensible empty or root results rather than errors.

### Normalization and relativity

```c
str_t path_normalize(Allocator *a, str_t path);  /* resolve . and .. */
str_t path_absolute(Allocator *a, str_t path);   /* relative → absolute using cwd */
str_t path_relative(Allocator *a, str_t from, str_t to);
bool  path_is_absolute(str_t path);
bool  path_is_relative(str_t path);
```

`path_normalize` cleans `.` and `..` components without resolving symlinks.  
`path_absolute` and `path_relative` perform the common conversions while remaining pure string operations where possible.

### Extension helpers

```c
bool  path_has_extension(str_t path, str_t ext);
str_t path_with_extension(Allocator *a, str_t path, str_t new_ext);
str_t path_without_extension(Allocator *a, str_t path);
```

These make the common “change or strip the extension” pattern trivial and allocation-aware.

---

## Existence and type checks `file_*`

```c
bool file_exists(str_t path);
bool file_is_file(str_t path);
bool file_is_dir(str_t path);
bool file_is_symlink(str_t path);
bool file_is_readable(str_t path);
bool file_is_writable(str_t path);
bool file_is_executable(str_t path);
bool file_is_empty(str_t path);          /* size == 0 or empty directory */
```

All of these are thin, predictable wrappers. They return `false` on any error (including “does not exist”) so the common “does this usable thing exist?” question is a single call.

---

## Metadata

```c
size_t file_size(str_t path);            /* 0 on error or missing */
time_t file_mtime(str_t path);
time_t file_atime(str_t path);
time_t file_ctime(str_t path);
mode_t file_mode(str_t path);
uid_t  file_owner(str_t path);
gid_t  file_group(str_t path);

typedef struct {
    size_t size;
    time_t mtime, atime, ctime;
    mode_t mode;
    bool   is_file;
    bool   is_dir;
    bool   is_symlink;
} FileInfo;

bool file_info(str_t path, FileInfo *out);
```

`file_info` fills a small value struct in one call, avoiding repeated stat traffic for the common case of wanting several pieces of metadata together.

---

## Reading and writing

```c
str_t  file_read(Allocator *a, str_t path);          /* binary-safe */
str_t  file_read_text(Allocator *a, str_t path);     /* null-terminated, text-oriented */
bool   file_write(str_t path, const void *data, size_t len);
bool   file_write_str(str_t path, str_t content);
bool   file_append(str_t path, const void *data, size_t len);
bool   file_append_str(str_t path, str_t content);

str_t *file_read_lines(Allocator *a, str_t path, size_t *out_count);
bool   file_write_lines(str_t path, str_t *lines, size_t count);
```

Whole-file helpers are the most common need. They allocate through the supplied allocator so the resulting `str_t` (or array of `str_t`) participates in the same lifetime model as the rest of Quasar.

---

## Directory operations

```c
bool file_mkdir(str_t path);             /* single level */
bool file_mkdir_p(str_t path);           /* recursive, like mkdir -p */
bool file_rmdir(str_t path);             /* empty directory only */
bool file_rmdir_r(str_t path);           /* recursive */

typedef struct {
    str_t  name;
    bool   is_dir;
    bool   is_symlink;
    size_t size;
} DirEntry;

DirEntry *file_list(Allocator *a, str_t path, size_t *out_count);
DirEntry *file_list_recursive(Allocator *a, str_t path, size_t *out_count);

bool   file_dir_is_empty(str_t path);
size_t file_dir_count(str_t path);       /* non-recursive entry count */
```

Directory listings return an allocator-owned array of `DirEntry`. The recursive variant walks the tree; the non-recursive variant is the cheap everyday case.

---

## Copy, move, and remove

```c
bool file_copy(str_t src, str_t dst);
bool file_copy_r(str_t src, str_t dst);   /* recursive */
bool file_move(str_t src, str_t dst);    /* rename or cross-device copy+remove */
bool file_remove(str_t path);            /* file or empty directory */
bool file_remove_r(str_t path);          /* recursive, anything */
```

Recursive variants are explicitly named so the cost is visible at the call site.

---

## Symlinks

```c
bool  file_symlink(str_t target, str_t linkpath);
str_t file_readlink(Allocator *a, str_t path);
str_t file_realpath(Allocator *a, str_t path);   /* resolve all symlinks + normalize */
```

`file_realpath` produces a canonical absolute path with all intermediate symlinks resolved.

---

## Working directory and special paths

```c
str_t file_cwd(Allocator *a);
bool  file_chdir(str_t path);

str_t file_home(Allocator *a);           /* $HOME or platform equivalent */
str_t file_temp_dir(Allocator *a);       /* system temporary directory */
str_t file_config_dir(Allocator *a);     /* XDG_CONFIG_HOME or equivalent */
str_t file_data_dir(Allocator *a);       /* XDG_DATA_HOME or equivalent */
```

These return allocator-owned strings so callers can treat them uniformly with other Quasar paths.

---

## Temporary files and directories

```c
str_t file_temp_file(Allocator *a);              /* unique empty file */
str_t file_temp_dir_create(Allocator *a);        /* unique directory */
bool  file_temp_cleanup(str_t path);             /* remove if still under temp */
```

Temporary helpers create uniquely named entries under the system temporary directory and return the resulting path owned by the allocator.

---

## Permissions and ownership

```c
bool file_chmod(str_t path, mode_t mode);
bool file_chown(str_t path, uid_t uid, gid_t gid);
bool file_chmod_r(str_t path, mode_t mode);      /* recursive */
```

Thin, explicit wrappers. Recursive permission changes are opt-in.

---

## Convenience operations

```c
bool file_touch(str_t path);                     /* create if missing, update mtime */

bool file_write_atomic(str_t path, const void *data, size_t len);
/* write to a temporary file in the same directory, then rename */

typedef bool (*FileWalkFn)(str_t path, const FileInfo *info, void *userdata);
bool file_walk(str_t root, FileWalkFn fn, void *userdata);
```

`file_write_atomic` is the safe “replace file contents” pattern.  
`file_walk` provides a non-allocating callback-style traversal for cases where a full `DirEntry` array is unnecessary.

---

## Compatibility with libc

Quasar filesystem helpers are built on the normal POSIX interfaces (`open`, `stat`, `mkdir`, `readdir`, `rename`, `unlink`, `symlink`, `readlink`, `realpath`, etc.).

They do not replace those interfaces. Programs may freely mix raw POSIX calls with Quasar helpers, provided ownership rules are respected:

- Memory obtained from a Quasar function must be managed through its allocator.
- File descriptors obtained from raw `open` remain the caller’s responsibility.

Quasar’s goal is to make the common higher-level patterns less repetitive, not to hide the underlying system.

---

## Design goals

The filesystem API is designed around the following goals:

- Provide a consistent, predictable interface for everyday path and file work.
- Integrate cleanly with Quasar’s allocator and `str_t` model.
- Keep path manipulation pure (no hidden I/O).
- Make recursive versus non-recursive operations explicit.
- Prefer simple boolean results for operations while still allowing `errno` inspection.
- Avoid forcing every program to re-implement the same thin wrappers.
- Remain close enough to POSIX that the mapping is obvious.
- Keep the surface small enough to learn quickly, yet complete enough that most programs need nothing else for ordinary filesystem tasks.

The functions above form the initial public surface intended for `std/files.h` (and the pure path helpers that may live alongside it). Individual signatures and exact edge-case behaviour will be refined as the implementation lands, but the shape and naming conventions are intended to stay stable.

```
