#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../core/mem.h"
#include <quasar/core/proc.h>
#include <quasar/core/str.h>
#include <quasar/std/process.h>
#include <quasar/std/strings.h>

/* ---- Process identity ---- */

pid_t
proc_getpid(void)
{
	return getpid();
}

pid_t
proc_getppid(void)
{
	return getppid();
}

uid_t
proc_getuid(void)
{
	return getuid();
}

gid_t
proc_getgid(void)
{
	return getgid();
}

uid_t
proc_geteuid(void)
{
	return geteuid();
}

gid_t
proc_getegid(void)
{
	return getegid();
}

str_t
proc_name(Allocator *a)
{
	str_t empty = {NULL, 0};
	FILE *fp;
	char buf[256];
	size_t len;

	if (!a) return empty;

	fp = fopen("/proc/self/comm", "r");
	if (!fp) return empty;

	if (!fgets(buf, sizeof(buf), fp)) {
		fclose(fp);
		return empty;
	}
	fclose(fp);

	len = strlen(buf);
	/* Strip trailing newline. */
	if (len > 0 && buf[len - 1] == '\n')
		len--;

	return str_from_n(a, buf, len);
}

str_t
proc_exe(Allocator *a)
{
	str_t empty = {NULL, 0};
	char buf[PATH_MAX];
	ssize_t n;

	if (!a) return empty;

	n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (n < 0) return empty;

	buf[n] = '\0';
	return str_from(a, buf);
}

/* ---- Spawning and execution ---- */

pid_t
proc_spawn(const ProcSpawnOpts *opts)
{
	Allocator *tmp_arena;
	struct Allocator *a;
	char **argv = NULL;
	char **envp = NULL;
	const char *cwd = NULL;
	int fds[3];
	size_t i;
	pid_t result = -1;

	if (!opts || !opts->argv || opts->argc == 0) {
		errno = EINVAL;
		return -1;
	}

	/* Validate path when not using PATH lookup. */
	if (!opts->search_path && opts->path.len == 0) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Use a temporary arena for the argv/envp C-string pointer
	 * arrays.  These only need to live until fork() completes.
	 */
	tmp_arena = mem_allocators_arena();
	if (!tmp_arena) return -1;
	a = (struct Allocator *)tmp_arena;

	/*
	 * Build the argv array as null-terminated C strings.
	 * Allocate argc + 1 pointers for the null terminator.
	 */
	argv = _quasar_mem_allocate(a, (opts->argc + 1) * sizeof(char *));
	if (!argv) goto done;

	for (i = 0; i < opts->argc; i++)
		argv[i] = (char *)str_cstr(opts->argv[i]);
	argv[opts->argc] = NULL;

	/* Build envp if provided. */
	if (opts->envp && opts->envc > 0) {
		envp = _quasar_mem_allocate(a, (opts->envc + 1) * sizeof(char *));
		if (!envp) goto done;
		for (i = 0; i < opts->envc; i++)
			envp[i] = (char *)str_cstr(opts->envp[i]);
		envp[opts->envc] = NULL;
	}

	/* Working directory. */
	if (opts->cwd.len > 0)
		cwd = str_cstr(opts->cwd);

	/* File descriptors. */
	fds[0] = opts->stdin_fd;
	fds[1] = opts->stdout_fd;
	fds[2] = opts->stderr_fd;

	/*
	 * If search_path is set, we need to use execvp semantics.
	 * proc_raw_spawn always uses execv/execve, so for PATH lookup
	 * we fork ourselves and call execvp in the child, bypassing
	 * proc_raw_spawn.
	 */
	if (opts->search_path) {
		pid_t pid = fork();
		if (pid < 0) {
			result = -1;
			goto done;
		}
		if (pid == 0) {
			if (fds[0] >= 0) {
				if (dup2(fds[0], STDIN_FILENO) < 0)
					_exit(127);
				if (fds[0] != STDIN_FILENO)
					close(fds[0]);
			}
			if (fds[1] >= 0) {
				if (dup2(fds[1], STDOUT_FILENO) < 0)
					_exit(127);
				if (fds[1] != STDOUT_FILENO)
					close(fds[1]);
			}
			if (fds[2] >= 0) {
				if (dup2(fds[2], STDERR_FILENO) < 0)
					_exit(127);
				if (fds[2] != STDERR_FILENO)
					close(fds[2]);
			}
			if (cwd) {
				if (chdir(cwd) < 0)
					_exit(127);
			}
			if (envp)
				execvpe(argv[0], argv, envp);
			else
				execvp(argv[0], argv);
			_exit(127);
		}
		result = pid;
	} else {
		result = proc_raw_spawn(str_cstr(opts->path),
					argv, envp, cwd, fds);
	}

done:
	mem_allocator_free(tmp_arena);
	return result;
}

pid_t
proc_spawn_simple(str_t path, str_t *argv, size_t argc)
{
	ProcSpawnOpts opts;

	opts.path        = path;
	opts.argv        = argv;
	opts.argc        = argc;
	opts.envp        = NULL;
	opts.envc        = 0;
	opts.cwd         = (str_t){NULL, 0};
	opts.stdin_fd    = -1;
	opts.stdout_fd   = -1;
	opts.stderr_fd   = -1;
	opts.search_path = false;

	return proc_spawn(&opts);
}

	bool
proc_exec(str_t path, str_t *argv, size_t argc)
{
	Allocator *tmp_arena;
	struct Allocator *a;
	char **cargv;
	size_t i;

	if (!argv || argc == 0) {
		errno = EINVAL;
		return false;
	}

	tmp_arena = mem_allocators_arena();
	if (!tmp_arena) {
		errno = ENOMEM;
		return false;
	}
	a = (struct Allocator *)tmp_arena;

	cargv = _quasar_mem_allocate(a, (argc + 1) * sizeof(char *));
	if (!cargv) {
		mem_allocator_free(tmp_arena);
		errno = ENOMEM;
		return false;
	}

	for (i = 0; i < argc; i++)
		cargv[i] = (char *)str_cstr(argv[i]);
	cargv[argc] = NULL;

	execv(str_cstr(path), cargv);
	mem_allocator_free(tmp_arena);
	return false;
}

bool
proc_exec_env(str_t path, str_t *argv, size_t argc,
	      str_t *envp, size_t envc)
{
	Allocator *tmp_arena;
	struct Allocator *a;
	char **cargv;
	char **cenvp;
	size_t i;

	if (!argv || argc == 0) {
		errno = EINVAL;
		return false;
	}

	tmp_arena = mem_allocators_arena();
	if (!tmp_arena) {
		errno = ENOMEM;
		return false;
	}
	a = (struct Allocator *)tmp_arena;

	cargv = _quasar_mem_allocate(a, (argc + 1) * sizeof(char *));
	if (!cargv) {
		mem_allocator_free(tmp_arena);
		errno = ENOMEM;
		return false;
	}

	for (i = 0; i < argc; i++)
		cargv[i] = (char *)str_cstr(argv[i]);
	cargv[argc] = NULL;

	cenvp = NULL;
	if (envp && envc > 0) {
		cenvp = _quasar_mem_allocate(a, (envc + 1) * sizeof(char *));
		if (!cenvp) {
			mem_allocator_free(tmp_arena);
			errno = ENOMEM;
			return false;
		}
		for (i = 0; i < envc; i++)
			cenvp[i] = (char *)str_cstr(envp[i]);
		cenvp[envc] = NULL;
	}

	execve(str_cstr(path), cargv, cenvp);
	mem_allocator_free(tmp_arena);
	return false;
}

pid_t
proc_system(Allocator *a, str_t command)
{
	Allocator *tmp_arena;
	pid_t pid;
	str_t argv[4];

	if (command.len == 0) {
		errno = EINVAL;
		return -1;
	}

	tmp_arena = mem_allocators_arena();
	if (!tmp_arena) {
		errno = ENOMEM;
		return -1;
	}

	argv[0] = str_from(tmp_arena, "/bin/sh");
	if (!argv[0].data) goto fail;

	argv[1] = str_from(tmp_arena, "-c");
	if (!argv[1].data) goto fail;

	argv[2] = str_dup(tmp_arena, command);
	if (!argv[2].data) goto fail;

	pid = proc_spawn_simple(argv[0], argv, 3);
	mem_allocator_free(tmp_arena);
	return pid;

fail:
	mem_allocator_free(tmp_arena);
	return -1;
}

/* ---- Waiting and status ---- */

static void
_decode_status(int wstatus, ProcStatus *out)
{
	out->exited      = WIFEXITED(wstatus);
	out->signaled    = WIFSIGNALED(wstatus);
	out->stopped     = WIFSTOPPED(wstatus);
	out->continued   = WIFCONTINUED(wstatus);
	out->exit_code   = out->exited    ? WEXITSTATUS(wstatus) : -1;
	out->term_signal = out->signaled  ? WTERMSIG(wstatus)    : -1;
}

bool
proc_wait(pid_t pid, ProcStatus *out)
{
	int wstatus;
	pid_t r;

	do {
		r = waitpid(pid, &wstatus, 0);
	} while (r < 0 && errno == EINTR);

	if (r < 0)
		return false;

	if (out) _decode_status(wstatus, out);
	return true;
}

bool
proc_wait_timeout(pid_t pid, ProcStatus *out, int ms)
{
	struct timespec start, now;
	int wstatus;
	pid_t result;
	int elapsed_ms;

	if (ms < 0) ms = 0;

	clock_gettime(CLOCK_MONOTONIC, &start);

	for (;;) {
		result = waitpid(pid, &wstatus, WNOHANG);
		if (result < 0) {
			if (errno == ECHILD)
				return false;
			if (errno == EINTR)
				continue;
			return false;
		}

		if (result > 0) {
			if (out) _decode_status(wstatus, out);
			return true;
		}

		clock_gettime(CLOCK_MONOTONIC, &now);
		elapsed_ms = (int)((now.tv_sec - start.tv_sec) * 1000 +
				  (now.tv_nsec - start.tv_nsec) / 1000000);

		if (elapsed_ms >= ms)
			return false;

		{
			struct timespec ts;
			ts.tv_sec  = 0;
			ts.tv_nsec = 10000000;
			nanosleep(&ts, NULL);
		}
	}
}

bool
proc_try_wait(pid_t pid, ProcStatus *out)
{
	int wstatus;
	pid_t result;

	do {
		result = waitpid(pid, &wstatus, WNOHANG);
	} while (result < 0 && errno == EINTR);

	if (result <= 0)
		return false;

	if (out) _decode_status(wstatus, out);
	return true;
}

bool
proc_wait_any(ProcStatus *out, pid_t *out_pid)
{
	int wstatus;
	pid_t result;

	do {
		result = waitpid(-1, &wstatus, 0);
	} while (result < 0 && errno == EINTR);

	if (result < 0)
		return false;

	if (out) _decode_status(wstatus, out);
	if (out_pid) *out_pid = result;
	return true;
}

bool
proc_wait_all(void)
{
	int wstatus;

	for (;;) {
		pid_t result = waitpid(-1, &wstatus, 0);
		if (result < 0) {
			if (errno == ECHILD)
				return true;
			if (errno == EINTR)
				continue;
			return false;
		}
	}
}

int
proc_exit_code(const ProcStatus *st)
{
	if (!st) return -1;
	return st->exit_code;
}

bool
proc_succeeded(const ProcStatus *st)
{
	if (!st) return false;
	return st->exited && st->exit_code == 0;
}

bool
proc_failed(const ProcStatus *st)
{
	if (!st) return true;
	if (st->exited) return st->exit_code != 0;
	if (st->signaled) return true;
	return false;
}

/* ---- Signals and termination ---- */

bool
proc_kill(pid_t pid, int sig)
{
	return kill(pid, sig) == 0;
}

bool
proc_terminate(pid_t pid)
{
	return proc_kill(pid, SIGTERM);
}

bool
proc_kill_force(pid_t pid)
{
	return proc_kill(pid, SIGKILL);
}

bool
proc_interrupt(pid_t pid)
{
	return proc_kill(pid, SIGINT);
}

bool
proc_signal_self(int sig)
{
	return kill(getpid(), sig) == 0;
}

bool
proc_raise(int sig)
{
	return raise(sig) == 0;
}

bool
proc_ignore_signal(int sig)
{
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	return sigaction(sig, &sa, NULL) == 0;
}

bool
proc_default_signal(int sig)
{
	struct sigaction sa;

	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	return sigaction(sig, &sa, NULL) == 0;
}

bool
proc_block_signal(int sig)
{
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, sig);
	return sigprocmask(SIG_BLOCK, &set, NULL) == 0;
}

bool
proc_unblock_signal(int sig)
{
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, sig);
	return sigprocmask(SIG_UNBLOCK, &set, NULL) == 0;
}

bool
proc_kill_group(pid_t pgid, int sig)
{
	return kill(-pgid, sig) == 0;
}

bool
proc_terminate_group(pid_t pgid)
{
	return proc_kill_group(pgid, SIGTERM);
}

/* ---- Pipes and redirection ---- */

bool
proc_pipe(ProcPipe *out)
{
	int fds[2];

	if (!out) {
		errno = EINVAL;
		return false;
	}

	if (pipe(fds) < 0)
		return false;

	out->read_fd  = fds[0];
	out->write_fd = fds[1];
	return true;
}

bool
proc_pipe_close(ProcPipe *p)
{
	if (!p) {
		errno = EINVAL;
		return false;
	}

	if (p->read_fd >= 0) {
		close(p->read_fd);
		p->read_fd = -1;
	}
	if (p->write_fd >= 0) {
		close(p->write_fd);
		p->write_fd = -1;
	}
	return true;
}

bool
proc_redirect_stdin(pid_t pid, int fd)
{
	/*
	 * Not applicable at std level — fd redirection is handled
	 * during spawn via ProcSpawnOpts.  This function exists for
	 * API completeness but cannot redirect an already-running
	 * child's fd on Linux without /proc tricks that are fragile
	 * and not universally available.
	 */
	(void)pid;
	(void)fd;
	errno = ENOSYS;
	return false;
}

bool
proc_redirect_stdout(pid_t pid, int fd)
{
	(void)pid;
	(void)fd;
	errno = ENOSYS;
	return false;
}

bool
proc_redirect_stderr(pid_t pid, int fd)
{
	(void)pid;
	(void)fd;
	errno = ENOSYS;
	return false;
}

str_t
proc_read_fd(Allocator *a, int fd, size_t max)
{
	str_t empty = {NULL, 0};
	Allocator *tmp;
	struct Allocator *ta;
	unsigned char *buf;
	str_t result;
	size_t total;
	ssize_t n;

	if (!a || fd < 0) return empty;

	if (max == SIZE_MAX) return empty;

	/*
	 * Use a temporary arena for the read buffer so the caller's
	 * allocator is not polluted with a double allocation.
	 */
	tmp = mem_allocators_arena();
	if (!tmp) return empty;
	ta = (struct Allocator *)tmp;

	buf = _quasar_mem_allocate(ta, max + 1);
	if (!buf) {
		mem_allocator_free(tmp);
		return empty;
	}

	total = 0;
	while (total < max) {
		n = read(fd, buf + total, max - total);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (n == 0)
			break;
		total += (size_t)n;
	}

	buf[total] = '\0';
	result = str_from_n(a, (const char *)buf, total);
	mem_allocator_free(tmp);
	return result;
}

bool
proc_write_fd(int fd, const void *data, size_t len)
{
	const unsigned char *buf;
	size_t remaining;
	ssize_t n;

	if (fd < 0 || !data) {
		errno = EINVAL;
		return false;
	}

	buf       = (const unsigned char *)data;
	remaining = len;

	while (remaining > 0) {
		n = write(fd, buf, remaining);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		buf       += (size_t)n;
		remaining -= (size_t)n;
	}

	return true;
}

bool
proc_write_str(int fd, str_t s)
{
	if (!s.data) return true; /* empty string: nothing to write */
	return proc_write_fd(fd, s.data, s.len);
}

/* ---- Environment ---- */

extern char **environ;

str_t
proc_getenv(Allocator *a, str_t key)
{
	str_t empty = {NULL, 0};
	const char *value;

	if (!a || key.len == 0 || !key.data) return empty;

	value = getenv(str_cstr(key));
	if (!value) return empty;

	return str_from(a, value);
}

bool
proc_setenv(str_t key, str_t value)
{
	if (key.len == 0 || !key.data ||
	    value.len == 0 || !value.data) {
		errno = EINVAL;
		return false;
	}
	return setenv(str_cstr(key), str_cstr(value), 1) == 0;
}

bool
proc_unsetenv(str_t key)
{
	if (key.len == 0 || !key.data) {
		errno = EINVAL;
		return false;
	}
	return unsetenv(str_cstr(key)) == 0;
}

bool
proc_clearenv(void)
{
	return clearenv() == 0;
}

str_t *
proc_environ(Allocator *a, size_t *out_count)
{
	size_t count;
	str_t *result;
	size_t i;

	if (out_count) *out_count = 0;

	if (!a || !environ) return NULL;

	/* Count entries. */
	count = 0;
	for (char **ep = environ; *ep != NULL; ep++)
		count++;

	result = _quasar_mem_allocate(a, count * sizeof(str_t));
	if (!result) return NULL;

	for (i = 0; i < count; i++) {
		result[i] = str_from(a, environ[i]);
		if (!result[i].data) {
			/* Partial failure — still return what we have. */
			if (out_count) *out_count = i;
			return result;
		}
	}

	if (out_count) *out_count = count;
	return result;
}

/* ---- Working directory and credentials ---- */

str_t
proc_getcwd(Allocator *a)
{
	str_t empty = {NULL, 0};
	char *buf;

	if (!a) return empty;

	buf = getcwd(NULL, 0);
	if (!buf) return empty;

	{
		str_t result = str_from(a, buf);
		free(buf);
		return result;
	}
}

bool
proc_chdir(str_t path)
{
	if (path.len == 0 || !path.data) {
		errno = EINVAL;
		return false;
	}
	return chdir(str_cstr(path)) == 0;
}

bool
proc_setuid(uid_t uid)
{
	return setuid(uid) == 0;
}

bool
proc_setgid(gid_t gid)
{
	return setgid(gid) == 0;
}

bool
proc_seteuid(uid_t uid)
{
	return seteuid(uid) == 0;
}

bool
proc_setegid(gid_t gid)
{
	return setegid(gid) == 0;
}

/* ---- Daemon and session ---- */

bool
proc_daemonize(void)
{
	pid_t pid;

	/* First fork: detach from controlling terminal. */
	pid = fork();
	if (pid < 0)
		return false;
	if (pid > 0)
		_exit(0); /* Parent exits. */

	/* Child: create new session. */
	if (setsid() < 0)
		return false;

	/* Second fork: ensure we can never reacquire a controlling terminal. */
	pid = fork();
	if (pid < 0)
		return false;
	if (pid > 0)
		_exit(0); /* First child exits. */

	/* Grandchild: the daemon. */
	if (chdir("/") < 0)
		return false;

	/* Close standard file descriptors and redirect to /dev/null. */
	{
		int fd = open("/dev/null", O_RDWR);
		if (fd >= 0) {
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > 2) close(fd);
		} else {
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
		}
	}

	return true;
}

bool
proc_setsid(void)
{
	return setsid() != (pid_t)-1;
}

pid_t
proc_getsid(pid_t pid)
{
	return getsid(pid);
}

pid_t
proc_getpgrp(void)
{
	return getpgrp();
}

bool
proc_setpgrp(void)
{
	return setpgrp() == 0;
}

bool
proc_setpgid(pid_t pid, pid_t pgid)
{
	return setpgid(pid, pgid) == 0;
}

/* ---- Convenience helpers ---- */

str_t
proc_run_capture(Allocator *a, str_t path,
		 str_t *argv, size_t argc,
		 ProcStatus *out_status)
{
	str_t empty = {NULL, 0};
	int pipefd[2];
	pid_t child;
	ProcSpawnOpts opts;

	if (!a || !argv || argc == 0) return empty;

	if (pipe(pipefd) < 0) return empty;

	opts.path        = path;
	opts.argv        = argv;
	opts.argc        = argc;
	opts.envp        = NULL;
	opts.envc        = 0;
	opts.cwd         = empty;
	opts.stdin_fd    = -1;
	opts.stdout_fd   = pipefd[1];
	opts.stderr_fd   = -1;
	opts.search_path = false;

	child = proc_spawn(&opts);
	if (child < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return empty;
	}

	/* Parent: close write end, read from read end. */
	close(pipefd[1]);

	{
		/*
		 * Read the output in chunks to avoid pre-allocating
		 * an unbounded buffer.  Use a manual allocator resized
		 * as needed; convert to a str_t via the caller's
		 * allocator at the end.
		 */
		str_t result;
		Allocator *chunk_a = mem_allocators_manual(4096);
		struct Allocator *ca;
		ssize_t n;
		size_t total = 0;

		if (!chunk_a) {
			close(pipefd[0]);
			return empty;
		}
		ca = (struct Allocator *)chunk_a;

		for (;;) {
			unsigned char *buf;
			size_t need = total + 4096;

			mem_allocators_manual_realloc_set(chunk_a, need);
			if (ca->u.m.size < need) break;

			buf = ca->u.m.data;
			n = read(pipefd[0], buf + total, 4096);
			if (n < 0) {
				if (errno == EINTR) continue;
				break;
			}
			if (n == 0) break; /* EOF */
			total += (size_t)n;
			ca->u.m.used = total;
		}
		close(pipefd[0]);

		if (total > 0)
			result = str_from_n(a, (const char *)ca->u.m.data, total);
		else
			result = str_empty(a);
		mem_allocator_free(chunk_a);

		{
			ProcStatus st;
			if (proc_wait(child, &st)) {
				if (out_status) *out_status = st;
			} else {
				if (out_status)
					memset(out_status, 0, sizeof(*out_status));
			}
		}

		return result;
	}
}

bool
proc_run(str_t path, str_t *argv, size_t argc)
{
	int nullfd;
	pid_t child;
	ProcSpawnOpts opts;
	str_t empty = {NULL, 0};
	ProcStatus st;

	if (!argv || argc == 0) {
		errno = EINVAL;
		return false;
	}

	/* Open /dev/null for stdout/stderr redirection. */
	nullfd = open("/dev/null", O_WRONLY);
	if (nullfd < 0) return false;

	opts.path        = path;
	opts.argv        = argv;
	opts.argc        = argc;
	opts.envp        = NULL;
	opts.envc        = 0;
	opts.cwd         = empty;
	opts.stdin_fd    = -1;
	opts.stdout_fd   = nullfd;
	opts.stderr_fd   = nullfd;
	opts.search_path = false;

	child = proc_spawn(&opts);

	/*
	 * Close nullfd in parent — the child has its own dup2'd copy.
	 */
	close(nullfd);

	if (child < 0)
		return false;

	if (!proc_wait(child, &st))
		return false;

	return proc_succeeded(&st);
}

str_t
proc_shell_capture(Allocator *a, str_t command,
		   ProcStatus *out_status)
{
	str_t empty = {NULL, 0};
	Allocator *tmp_arena;
	str_t argv[4];
	str_t result;

	if (!a || command.len == 0) return empty;

	tmp_arena = mem_allocators_arena();
	if (!tmp_arena) return empty;

	argv[0] = str_from(tmp_arena, "/bin/sh");
	if (!argv[0].data) goto fail;

	argv[1] = str_from(tmp_arena, "-c");
	if (!argv[1].data) goto fail;

	argv[2] = str_dup(tmp_arena, command);
	if (!argv[2].data) goto fail;

	argv[3] = empty;

	result = proc_run_capture(a, argv[0], argv, 3, out_status);
	mem_allocator_free(tmp_arena);
	return result;

fail:
	mem_allocator_free(tmp_arena);
	return empty;
}

bool
proc_shell(str_t command)
{
	str_t empty = {NULL, 0};
	str_t tmp_alloc_buf[4];
	Allocator *tmp_arena;
	bool result;

	if (command.len == 0) return false;

	/*
	 * Use a temporary arena for the argv strings so they outlive
	 * the call without leaking.
	 */
	tmp_arena = mem_allocators_arena();
	if (!tmp_arena) return false;

	tmp_alloc_buf[0] = str_from(tmp_arena, "/bin/sh");
	tmp_alloc_buf[1] = str_from(tmp_arena, "-c");
	tmp_alloc_buf[2] = str_dup(tmp_arena, command);
	tmp_alloc_buf[3] = empty;

	if (!tmp_alloc_buf[0].data || !tmp_alloc_buf[1].data ||
	    !tmp_alloc_buf[2].data) {
		mem_allocator_free(tmp_arena);
		return false;
	}

	result = proc_run(tmp_alloc_buf[0], tmp_alloc_buf, 3);
	mem_allocator_free(tmp_arena);
	return result;
}

bool
proc_is_alive(pid_t pid)
{
	if (kill(pid, 0) == 0)
		return true;
	return errno == EPERM;
}

void
proc_sleep_ms(int ms)
{
	if (ms <= 0) return;

	{
		struct timespec ts;
		ts.tv_sec  = ms / 1000;
		ts.tv_nsec = (ms % 1000) * 1000000L;
		nanosleep(&ts, NULL);
	}
}

void
proc_sleep_s(int seconds)
{
	if (seconds <= 0) return;
	sleep((unsigned int)seconds);
}
