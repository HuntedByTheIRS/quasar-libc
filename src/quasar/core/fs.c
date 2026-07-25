#define _XOPEN_SOURCE 700
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#include <quasar/core/fs.h>

/* ------------------------------------------------------------------ */
/*  Static helpers                                                     */
/* ------------------------------------------------------------------ */

/*
 * Fill a QsFileStat from a struct stat.
 * Caller must ensure st is valid (stat/lstat succeeded).
 */
static void
fill_stat(const struct stat *st, QsFileStat *out)
{
	out->size      = (size_t)st->st_size;
	out->mtime     = st->st_mtime;
	out->atime     = st->st_atime;
	out->ctime     = st->st_ctime;
	out->mode      = st->st_mode;
	out->uid       = st->st_uid;
	out->gid       = st->st_gid;
	out->dev       = st->st_dev;
	out->ino       = st->st_ino;
	out->is_file   = S_ISREG(st->st_mode);
	out->is_dir    = S_ISDIR(st->st_mode);
	out->is_symlink = S_ISLNK(st->st_mode);
}

/*
 * Write len bytes from data to fd, retrying on EINTR.
 * Returns true only when every byte is written.
 */
static bool
write_all(int fd, const void *data, size_t len)
{
	const unsigned char *p = data;
	size_t remaining = len;

	while (remaining > 0) {
		ssize_t n = write(fd, p, remaining);
		if (n < 0) {
			if (errno == EINTR) continue;
			return false;
		}
		p += (size_t)n;
		remaining -= (size_t)n;
	}
	return true;
}

/*
 * Read up to len bytes from fd into buf, retrying on EINTR.
 * Returns the total number of bytes read, or -1 on error.
 * Short reads (including EOF) are handled correctly —
 * the return value reflects actual bytes transferred.
 */
static ssize_t
read_loop(int fd, void *buf, size_t len)
{
	unsigned char *p = buf;
	size_t remaining = len;

	while (remaining > 0) {
		ssize_t n = read(fd, p, remaining);
		if (n < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		if (n == 0) break; /* EOF */
		p += (size_t)n;
		remaining -= (size_t)n;
	}
	return (ssize_t)(len - remaining);
}

/* ---- dynamic-entry-list helpers (used by _quasar_fs_list_recursive) ---- */

typedef struct {
	QsDirEntry *entries;
	size_t      count;
	size_t      cap;
} DirList;

static bool
dirlist_init(DirList *dl)
{
	dl->cap   = 64;
	dl->count = 0;
	dl->entries = malloc(dl->cap * sizeof(QsDirEntry));
	return dl->entries != 0;
}

static bool
dirlist_add(DirList *dl, const char *name, bool is_dir,
            bool is_symlink, size_t size)
{
	QsDirEntry *new_entries;

	if (dl->count >= dl->cap) {
		size_t new_cap = dl->cap * 2;
		new_entries = realloc(dl->entries, new_cap * sizeof(QsDirEntry));
		if (!new_entries) return false;
		dl->entries = new_entries;
		dl->cap     = new_cap;
	}

	dl->entries[dl->count].name = strdup(name);
	if (!dl->entries[dl->count].name) return false;
	dl->entries[dl->count].is_dir    = is_dir;
	dl->entries[dl->count].is_symlink = is_symlink;
	dl->entries[dl->count].size      = size;
	dl->count++;
	return true;
}

static void
dirlist_free_partial(DirList *dl)
{
	size_t i;

	for (i = 0; i < dl->count; i++)
		free(dl->entries[i].name);
	free(dl->entries);
}

/*
 * Build a full path: prefix + "/" + name.
 * If prefix is empty or ".", just return a copy of name.
 * Returns a malloc'd string the caller must free, or NULL on OOM.
 */
static char *
path_join(const char *prefix, const char *name)
{
	size_t plen, nlen;
	char  *result;

	if (!prefix || prefix[0] == '\0' ||
	    (prefix[0] == '.' && prefix[1] == '\0'))
		return strdup(name);

	plen   = strlen(prefix);
	nlen   = strlen(name);
	result = malloc(plen + 1 + nlen + 1);
	if (!result) return 0;
	memcpy(result, prefix, plen);
	result[plen] = '/';
	memcpy(result + plen + 1, name, nlen + 1);
	return result;
}

/* forward decl for mutual recursion */
static bool list_recursive_impl(const char *base, const char *prefix,
                                DirList *dl);

/*
 * Recursive depth-first listing helper.
 * base   — absolute (or relative) root path for filesystem access.
 * prefix — relative prefix accumulated so far ("" at the top level).
 * dl     — dynamic entry list being built.
 */
static bool
list_recursive_impl(const char *base, const char *prefix, DirList *dl)
{
	char       *fulldir;
	size_t      count, i;
	QsDirEntry *entries;

	fulldir = path_join(base, prefix);
	if (!fulldir) return false;

	entries = _quasar_fs_list(fulldir, &count);
	free(fulldir);
	/*
	 * _quasar_fs_list returns non-NULL (even with count==0) for empty
	 * directories.  NULL means an actual error (opendir/readdir failure).
	 */
	if (!entries) return false;

	for (i = 0; i < count; i++) {
		char *rel;

		rel = path_join(prefix, entries[i].name);
		if (!rel) {
			_quasar_fs_list_free(entries, count);
			return false;
		}

		if (!dirlist_add(dl, rel, entries[i].is_dir,
		                 entries[i].is_symlink, entries[i].size)) {
			free(rel);
			_quasar_fs_list_free(entries, count);
			return false;
		}

		/*
		 * Recurse into subdirectories.  _quasar_fs_list uses lstat(),
		 * so symlinks-to-dirs are NOT followed — avoiding infinite
		 * loops from symlink cycles.
		 */
		if (entries[i].is_dir) {
			if (!list_recursive_impl(base, rel, dl)) {
				free(rel);
				_quasar_fs_list_free(entries, count);
				return false;
			}
		}
		free(rel);
	}

	_quasar_fs_list_free(entries, count);
	return true;
}

/* ------------------------------------------------------------------ */
/*  Stat operations                                                    */
/* ------------------------------------------------------------------ */

bool
_quasar_fs_stat(const char *path, QsFileStat *out)
{
	struct stat st;

	if (stat(path, &st) != 0) return false;
	fill_stat(&st, out);
	return true;
}

bool
_quasar_fs_lstat(const char *path, QsFileStat *out)
{
	struct stat st;

	if (lstat(path, &st) != 0) return false;
	fill_stat(&st, out);
	return true;
}

/* ------------------------------------------------------------------ */
/*  Existence and type checks                                          */
/* ------------------------------------------------------------------ */

bool
_quasar_fs_exists(const char *path)
{
	return access(path, F_OK) == 0;
}

bool
_quasar_fs_is_readable(const char *path)
{
	return access(path, R_OK) == 0;
}

bool
_quasar_fs_is_writable(const char *path)
{
	return access(path, W_OK) == 0;
}

bool
_quasar_fs_is_executable(const char *path)
{
	return access(path, X_OK) == 0;
}

bool
_quasar_fs_is_file(const char *path)
{
	struct stat st;

	if (lstat(path, &st) != 0) return false;
	return S_ISREG(st.st_mode);
}

bool
_quasar_fs_is_dir(const char *path)
{
	struct stat st;

	if (lstat(path, &st) != 0) return false;
	return S_ISDIR(st.st_mode);
}

bool
_quasar_fs_is_symlink(const char *path)
{
	struct stat st;

	if (lstat(path, &st) != 0) return false;
	return S_ISLNK(st.st_mode);
}

/* ------------------------------------------------------------------ */
/*  File I/O                                                           */
/* ------------------------------------------------------------------ */

char *
_quasar_fs_read(const char *path, size_t *out_len)
{
	int         fd;
	struct stat st;
	char       *buf;
	ssize_t     nread;
	size_t      buf_size, total;

	*out_len = (size_t)-1; /* sentinel: error unless overwritten */

	fd = open(path, O_RDONLY);
	if (fd < 0) return 0;

	if (fstat(fd, &st) != 0) {
		close(fd);
		return 0;
	}

	/*
	 * Use fstat size for regular files.  For virtual filesystems
	 * (/proc, /sys) that report st_size == 0 but contain data,
	 * start with a 4096-byte buffer and grow as needed.
	 */
	buf_size = (st.st_size > 0) ? (size_t)st.st_size : 4096;
	buf = malloc(buf_size);
	if (!buf) {
		close(fd);
		errno = ENOMEM;
		return 0;
	}

	total = 0;
	for (;;) {
		nread = read_loop(fd, buf + total, buf_size - total);
		if (nread < 0) {
			free(buf);
			close(fd);
			return 0;
		}
		total += (size_t)nread;
		if (total < buf_size) break; /* EOF or short read */

		/* Regular file: fstat told us the size, so we're done.
		 * Virtual file: buffer is full, there may be more. */
		if (st.st_size > 0) break;

		buf_size *= 2;
		{
			char *newbuf = realloc(buf, buf_size);
			if (!newbuf) {
				free(buf);
				close(fd);
				errno = ENOMEM;
				return 0;
			}
			buf = newbuf;
		}
	}

	close(fd);

	if (total == 0) {
		free(buf);
		*out_len = 0; /* empty file */
		return 0;
	}

	*out_len = total;
	return buf;
}

bool
_quasar_fs_write(const char *path, const void *data, size_t len)
{
	int  fd;
	bool ok;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) return false;

	ok = write_all(fd, data, len);
	close(fd);
	return ok;
}

bool
_quasar_fs_append(const char *path, const void *data, size_t len)
{
	int  fd;
	bool ok;

	fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd < 0) return false;

	ok = write_all(fd, data, len);
	close(fd);
	return ok;
}

/* ------------------------------------------------------------------ */
/*  Directory creation                                                 */
/* ------------------------------------------------------------------ */

bool
_quasar_fs_mkdir(const char *path)
{
	return mkdir(path, 0777) == 0;
}

bool
_quasar_fs_mkdir_p(const char *path)
{
	char  *buf;
	char  *p;
	size_t len;

	if (!path || path[0] == '\0') {
		errno = EINVAL;
		return false;
	}

	buf = strdup(path);
	if (!buf) return false;
	len = strlen(buf);

	/* Strip trailing slashes. */
	while (len > 1 && buf[len - 1] == '/')
		buf[--len] = '\0';

	p = buf;

	/* Skip leading slash(es) so the root "/" itself is preserved. */
	if (*p == '/') p++;

	while (1) {
		char  *sep;
		struct stat st;

		sep = strchr(p, '/');
		if (sep) *sep = '\0';

		if (mkdir(buf, 0777) != 0) {
			if (errno != EEXIST) {
				free(buf);
				return false;
			}
			/* Already exists — ensure it's a directory. */
			if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {
				free(buf);
				return false;
			}
		}

		if (!sep) break;
		*sep = '/';
		p    = sep + 1;
	}

	free(buf);
	return true;
}

bool
_quasar_fs_rmdir(const char *path)
{
	return rmdir(path) == 0;
}

bool
_quasar_fs_rmdir_r(const char *path)
{
	size_t      count, i;
	QsDirEntry *entries;

	entries = _quasar_fs_list(path, &count);
	if (!entries) {
		/* If listing failed, still try rmdir (might be empty). */
		return rmdir(path) == 0;
	}

	for (i = 0; i < count; i++) {
		char *full = path_join(path, entries[i].name);
		int   saved_errno = 0;

		if (!full) {
			_quasar_fs_list_free(entries, count);
			return false;
		}

		if (entries[i].is_dir) {
			if (!_quasar_fs_rmdir_r(full) && !saved_errno)
				saved_errno = errno;
		} else {
			if (unlink(full) != 0 && !saved_errno)
				saved_errno = errno;
		}

		if (saved_errno) {
			free(full);
			_quasar_fs_list_free(entries, count);
			errno = saved_errno;
			return false;
		}

		free(full);
	}

	_quasar_fs_list_free(entries, count);
	return rmdir(path) == 0;
}

/* ------------------------------------------------------------------ */
/*  Directory listing                                                  */
/* ------------------------------------------------------------------ */

QsDirEntry *
_quasar_fs_list(const char *path, size_t *out_count)
{
	DIR           *d;
	struct dirent *de;
	QsDirEntry    *entries = 0;
	size_t         count = 0, cap = 0;

	*out_count = 0;

	d = opendir(path);
	if (!d) return 0;

	cap = 32;
	entries = malloc(cap * sizeof(QsDirEntry));
	if (!entries) {
		closedir(d);
		return 0;
	}

	errno = 0;
	while ((de = readdir(d)) != 0) {
		QsDirEntry *new_entries;
		struct stat st;
		char       *full;

		/* Skip . and .. */
		if (de->d_name[0] == '.' &&
		    (de->d_name[1] == '\0' ||
		     (de->d_name[1] == '.' && de->d_name[2] == '\0')))
			continue;

		if (count >= cap) {
			cap *= 2;
			new_entries = realloc(entries, cap * sizeof(QsDirEntry));
			if (!new_entries) {
				size_t j;
				for (j = 0; j < count; j++)
					free(entries[j].name);
				free(entries);
				closedir(d);
				return 0;
			}
			entries = new_entries;
		}

		entries[count].name = strdup(de->d_name);
		if (!entries[count].name) {
			size_t j;
			for (j = 0; j < count; j++)
				free(entries[j].name);
			free(entries);
			closedir(d);
			return 0;
		}

		/* Stat to determine type and size.
		 * Use lstat so symlinks are not followed for type detection;
		 * is_dir/is_symlink will be accurate for the entry itself. */
		full = path_join(path, de->d_name);
		if (full && lstat(full, &st) == 0) {
			entries[count].is_dir    = S_ISDIR(st.st_mode);
			entries[count].is_symlink = S_ISLNK(st.st_mode);
			entries[count].size      = (size_t)st.st_size;
		} else {
			/* If we can't stat, default to file with size 0. */
			entries[count].is_dir    = false;
			entries[count].is_symlink = false;
			entries[count].size      = 0;
		}
		free(full);

		count++;
		errno = 0;
	}

	/* readdir finished; check for a real error. */
	if (errno != 0) {
		size_t j;
		for (j = 0; j < count; j++)
			free(entries[j].name);
		free(entries);
		closedir(d);
		*out_count = 0;
		return 0;
	}

	closedir(d);
	*out_count = count;
	return entries;
}

QsDirEntry *
_quasar_fs_list_recursive(const char *path, size_t *out_count)
{
	DirList dl;

	*out_count = 0;

	if (!dirlist_init(&dl)) return 0;

	if (!list_recursive_impl(path, "", &dl)) {
		dirlist_free_partial(&dl);
		return 0;
	}

	*out_count = dl.count;
	return dl.entries;
}

void
_quasar_fs_list_free(QsDirEntry *entries, size_t count)
{
	size_t i;

	if (!entries) return;
	for (i = 0; i < count; i++)
		free(entries[i].name);
	free(entries);
}

/* ------------------------------------------------------------------ */
/*  Copy, move, and remove                                             */
/* ------------------------------------------------------------------ */

bool
_quasar_fs_copy(const char *src, const char *dst)
{
	int     fd_src = -1, fd_dst = -1;
	bool    dst_opened = false;
	bool    ok = false;
	ssize_t nread;
	struct stat src_st;

	fd_src = open(src, O_RDONLY);
	if (fd_src < 0) return false;

	if (fstat(fd_src, &src_st) != 0) {
		close(fd_src);
		return false;
	}

	fd_dst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd_dst < 0) goto cleanup;
	dst_opened = true;

	{
		unsigned char buf[65536]; /* 64 KiB */

		for (;;) {
			nread = read_loop(fd_src, buf, sizeof(buf));
			if (nread < 0) goto cleanup;
			if (nread == 0) break; /* EOF */
			if (!write_all(fd_dst, buf, (size_t)nread))
				goto cleanup;
		}
	}

	fchmod(fd_dst, src_st.st_mode);
	ok = true;

cleanup:
	if (fd_src >= 0) close(fd_src);
	if (fd_dst >= 0) {
		close(fd_dst);
		if (!ok && dst_opened) unlink(dst);
	}
	return ok;
}

bool
_quasar_fs_copy_r(const char *src, const char *dst)
{
	QsFileStat st;

	if (!_quasar_fs_lstat(src, &st)) return false;

	if (st.is_dir) {
		size_t      count, i;
		QsDirEntry *entries;

		if (!_quasar_fs_mkdir_p(dst)) return false;

		entries = _quasar_fs_list(src, &count);
		if (!entries) return false;

		for (i = 0; i < count; i++) {
			char *src_child = path_join(src, entries[i].name);
			char *dst_child = path_join(dst, entries[i].name);

			if (!src_child || !dst_child) {
				free(src_child);
				free(dst_child);
				_quasar_fs_list_free(entries, count);
				return false;
			}

			if (!_quasar_fs_copy_r(src_child, dst_child)) {
				free(src_child);
				free(dst_child);
				_quasar_fs_list_free(entries, count);
				return false;
			}

			free(src_child);
			free(dst_child);
		}

		_quasar_fs_list_free(entries, count);
		return true;
	}

	if (st.is_symlink) {
		char *target = _quasar_fs_readlink(src);
		bool  ok;

		if (!target) return false;
		ok = _quasar_fs_symlink(target, dst);
		free(target);
		return ok;
	}

	return _quasar_fs_copy(src, dst);
}

bool
_quasar_fs_move(const char *src, const char *dst)
{
	if (rename(src, dst) == 0) return true;
	if (errno != EXDEV) return false;

	/* Cross-device — copy then remove. */
	if (!_quasar_fs_copy_r(src, dst)) return false;
	return _quasar_fs_remove_r(src);
}

bool
_quasar_fs_remove(const char *path)
{
	if (unlink(path) == 0) return true;
	if (errno == EISDIR || errno == EPERM)
		return rmdir(path) == 0;
	return false;
}

bool
_quasar_fs_remove_r(const char *path)
{
	QsFileStat st;

	/* Use lstat so we don't follow symlinks to directories. */
	if (!_quasar_fs_lstat(path, &st)) return false;

	if (st.is_dir && !st.is_symlink)
		return _quasar_fs_rmdir_r(path);

	return unlink(path) == 0;
}

/* ------------------------------------------------------------------ */
/*  Symlinks                                                           */
/* ------------------------------------------------------------------ */

bool
_quasar_fs_symlink(const char *target, const char *linkpath)
{
	return symlink(target, linkpath) == 0;
}

char *
_quasar_fs_readlink(const char *path)
{
	size_t  bufsz = 256;
	char   *buf;
	ssize_t n;

	buf = malloc(bufsz);
	if (!buf) return 0;

	while (1) {
		n = readlink(path, buf, bufsz);
		if (n < 0) {
			free(buf);
			return 0;
		}
		if ((size_t)n < bufsz) break; /* fit in buffer */

		/* Buffer too small — grow and retry. */
		bufsz *= 2;
		{
			char *nb = realloc(buf, bufsz);
			if (!nb) {
				free(buf);
				return 0;
			}
			buf = nb;
		}
	}

	buf[n] = '\0';
	return buf;
}

char *
_quasar_fs_realpath(const char *path)
{
	return realpath(path, 0);
}

/* ------------------------------------------------------------------ */
/*  Working directory                                                  */
/* ------------------------------------------------------------------ */

char *
_quasar_fs_cwd(void)
{
	return getcwd(0, 0);
}

bool
_quasar_fs_chdir(const char *path)
{
	return chdir(path) == 0;
}

/* ------------------------------------------------------------------ */
/*  Special paths                                                      */
/* ------------------------------------------------------------------ */

char *
_quasar_fs_home(void)
{
	const char *home;

	home = getenv("HOME");
	if (home && home[0] != '\0')
		return strdup(home);
	return strdup("/");
}

char *
_quasar_fs_temp_dir(void)
{
	const char *tmpdir;

	tmpdir = getenv("TMPDIR");
	if (tmpdir && tmpdir[0] != '\0')
		return strdup(tmpdir);
	return strdup("/tmp");
}

char *
_quasar_fs_config_dir(void)
{
	const char *xdg;
	char       *home;
	char       *result;
	size_t      hlen;

	xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && xdg[0] != '\0')
		return strdup(xdg);

	home = _quasar_fs_home();
	if (!home) return 0;

	hlen   = strlen(home);
	result = malloc(hlen + 9); /* "/.config" + NUL */
	if (!result) {
		free(home);
		return 0;
	}
	memcpy(result, home, hlen);
	memcpy(result + hlen, "/.config", 9);
	free(home);
	return result;
}

char *
_quasar_fs_data_dir(void)
{
	const char *xdg;
	char       *home;
	char       *result;
	size_t      hlen;

	xdg = getenv("XDG_DATA_HOME");
	if (xdg && xdg[0] != '\0')
		return strdup(xdg);

	home = _quasar_fs_home();
	if (!home) return 0;

	hlen   = strlen(home);
	result = malloc(hlen + 13); /* "/.local/share" + NUL */
	if (!result) {
		free(home);
		return 0;
	}
	memcpy(result, home, hlen);
	memcpy(result + hlen, "/.local/share", 13);
	free(home);
	return result;
}

/* ------------------------------------------------------------------ */
/*  Permissions                                                        */
/* ------------------------------------------------------------------ */

bool
_quasar_fs_chmod(const char *path, mode_t mode)
{
	return chmod(path, mode) == 0;
}

bool
_quasar_fs_chown(const char *path, uid_t uid, gid_t gid)
{
	/* On failure (typically EPERM as non-root), return false. */
	return chown(path, uid, gid) == 0;
}

/* ------------------------------------------------------------------ */
/*  Convenience                                                        */
/* ------------------------------------------------------------------ */

bool
_quasar_fs_touch(const char *path)
{
	struct stat st;
	int         fd;

	if (stat(path, &st) == 0) {
		/* File exists — update mtime to now.
		 * NULL times means "current time" for both atime and mtime. */
		return utimensat(AT_FDCWD, path, 0, 0) == 0;
	}

	/* Does not exist — create an empty file. */
	fd = open(path, O_WRONLY | O_CREAT, 0666);
	if (fd < 0) return false;
	close(fd);
	return true;
}

bool
_quasar_fs_write_atomic(const char *path, const void *data, size_t len)
{
	size_t  plen;
	char   *tmp;
	int     fd;
	bool    ok = false;

	plen = strlen(path);
	tmp = malloc(plen + 8); /* ".XXXXXX" + NUL */
	if (!tmp) return false;

	memcpy(tmp, path, plen);
	memcpy(tmp + plen, ".XXXXXX", 8);

	fd = mkstemp(tmp);
	if (fd < 0) {
		free(tmp);
		return false;
	}

	if (!write_all(fd, data, len))
		goto fail_write;

	if (close(fd) != 0) {
		fd = -1;
		goto fail_write;
	}
	fd = -1;

	if (rename(tmp, path) != 0) goto fail_rename;

	ok = true;
	goto done;

fail_write:
	if (fd >= 0) close(fd);
	unlink(tmp);
	goto done;

fail_rename:
	unlink(tmp);
	goto done;

done:
	free(tmp);
	return ok;
}
