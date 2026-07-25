#ifndef QUASAR_STD_PROCESS_H
#define QUASAR_STD_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include <quasar/core/str.h>
#include <quasar/std/memory.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * process.h — Quasar standard process management API.
 *
 * Provides practical, allocator-aware helpers for process management:
 * identity, spawning, waiting, signals, pipes, environment, working
 * directory, credentials, daemonization, and common convenience
 * patterns.
 *
 * All functions that return owned data take an Allocator* and return
 * memory owned by it.  Boolean-returning operations indicate success
 * or failure; on failure the caller may inspect errno.
 *
 * Process handles (pid_t) and status values are plain values.
 * File descriptors obtained from pipe helpers remain the caller's
 * responsibility.
 */

/* ── Process identity ────────────────────────────────────────────── */

/*
 * Return the current process ID.
 */
pid_t proc_getpid(void);

/*
 * Return the parent process ID.
 */
pid_t proc_getppid(void);

/*
 * Return the real user ID of the calling process.
 */
uid_t proc_getuid(void);

/*
 * Return the real group ID of the calling process.
 */
gid_t proc_getgid(void);

/*
 * Return the effective user ID of the calling process.
 */
uid_t proc_geteuid(void);

/*
 * Return the effective group ID of the calling process.
 */
gid_t proc_getegid(void);

/*
 * Return the current process name (comm).  The result is owned by
 * the supplied allocator.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t proc_name(Allocator *a);

/*
 * Return the path to the current executable (readlink of
 * /proc/self/exe).  The result is owned by the supplied allocator.
 *
 * Returns {NULL, 0} if a is NULL or allocation fails.
 */
str_t proc_exe(Allocator *a);

/* ── Spawning and execution ──────────────────────────────────────── */

/*
 * Options for proc_spawn().  All str_t fields that are empty
 * (len == 0) cause the corresponding child attribute to be
 * inherited from the parent.
 */
typedef struct {
	str_t  path;           /* executable path */
	str_t *argv;           /* argument vector (owned by caller) */
	size_t argc;
	str_t *envp;           /* optional environment; NULL = inherit */
	size_t envc;
	str_t  cwd;            /* optional working directory; empty = inherit */
	int    stdin_fd;       /* -1 = inherit */
	int    stdout_fd;
	int    stderr_fd;
	bool   search_path;    /* use PATH lookup */
} ProcSpawnOpts;

/*
 * Spawn a child process.  Returns the child pid on success,
 * or -1 on failure (errno is set).
 */
pid_t proc_spawn(const ProcSpawnOpts *opts);

/*
 * Spawn a child process with minimal arguments.  Returns the child
 * pid on success, or -1 on failure (errno is set).
 */
pid_t proc_spawn_simple(str_t path, str_t *argv, size_t argc);

/*
 * Replace the current process image.  Never returns on success.
 *
 * Returns false on failure (only reachable if exec fails); errno is set.
 */
bool proc_exec(str_t path, str_t *argv, size_t argc);

/*
 * Replace the current process image with an explicit environment.
 * Never returns on success.
 *
 * Returns false on failure; errno is set.
 */
bool proc_exec_env(str_t path, str_t *argv, size_t argc,
		   str_t *envp, size_t envc);

/*
 * Execute a shell command (via /bin/sh -c).  Returns the child pid
 * on success, or -1 on failure (errno is set).
 *
 * The command string is owned by the supplied allocator.
 */
pid_t proc_system(Allocator *a, str_t command);

/* ── Waiting and status ──────────────────────────────────────────── */

/*
 * Decoded process status.  Callers never need to decode raw wait
 * status bits themselves.
 */
typedef struct {
	int  exit_code;      /* meaningful only if exited normally */
	int  term_signal;    /* meaningful only if killed by signal */
	bool exited;
	bool signaled;
	bool stopped;
	bool continued;
} ProcStatus;

/*
 * Wait for a specific child process to change state (blocking).
 * Returns false if waitpid fails; errno is set.
 */
bool proc_wait(pid_t pid, ProcStatus *out);

/*
 * Wait for a specific child process with a timeout in milliseconds.
 * Returns false on timeout or error; errno is set on error.
 */
bool proc_wait_timeout(pid_t pid, ProcStatus *out, int ms);

/*
 * Wait for a specific child process (non-blocking).
 * Returns false if no child has changed state or on error;
 * check errno (ECHILD vs EAGAIN) to distinguish.
 */
bool proc_try_wait(pid_t pid, ProcStatus *out);

/*
 * Wait for any child process.  *out_pid receives the pid of the
 * child that changed state.
 * Returns false on error; errno is set (ECHILD = no children).
 */
bool proc_wait_any(ProcStatus *out, pid_t *out_pid);

/*
 * Wait for all remaining child processes.  Returns false if
 * there are no children or an error occurs.
 */
bool proc_wait_all(void);

/*
 * Return the exit code from a ProcStatus, or -1 if the process
 * did not exit normally.
 */
int proc_exit_code(const ProcStatus *st);

/*
 * Return true if the process exited with code 0.
 */
bool proc_succeeded(const ProcStatus *st);

/*
 * Return true if the process exited with a non-zero code
 * or was terminated by a signal.
 */
bool proc_failed(const ProcStatus *st);

/* ── Signals and termination ─────────────────────────────────────── */

/*
 * Send a signal to a process.  Returns false on failure; errno is set.
 */
bool proc_kill(pid_t pid, int sig);

/*
 * Send SIGTERM to a process.  Returns false on failure.
 */
bool proc_terminate(pid_t pid);

/*
 * Send SIGKILL to a process.  Returns false on failure.
 */
bool proc_kill_force(pid_t pid);

/*
 * Send SIGINT to a process.  Returns false on failure.
 */
bool proc_interrupt(pid_t pid);

/*
 * Send a signal to the calling process.  Returns false on failure.
 */
bool proc_signal_self(int sig);

/*
 * Send a signal to the calling process (alias for raise(3)).
 * Returns false on failure.
 */
bool proc_raise(int sig);

/*
 * Set the disposition of a signal to SIG_IGN.
 * Returns false on failure; errno is set.
 */
bool proc_ignore_signal(int sig);

/*
 * Set the disposition of a signal to SIG_DFL.
 * Returns false on failure; errno is set.
 */
bool proc_default_signal(int sig);

/*
 * Block a signal (add it to the process signal mask).
 * Returns false on failure; errno is set.
 */
bool proc_block_signal(int sig);

/*
 * Unblock a signal (remove it from the process signal mask).
 * Returns false on failure; errno is set.
 */
bool proc_unblock_signal(int sig);

/*
 * Send a signal to a process group.  Returns false on failure.
 */
bool proc_kill_group(pid_t pgid, int sig);

/*
 * Send SIGTERM to a process group.  Returns false on failure.
 */
bool proc_terminate_group(pid_t pgid);

/* ── Pipes and redirection ───────────────────────────────────────── */

/*
 * A pair of file descriptors representing a pipe.
 */
typedef struct {
	int read_fd;
	int write_fd;
} ProcPipe;

/*
 * Create a pipe.  Returns false on failure; errno is set.
 */
bool proc_pipe(ProcPipe *out);

/*
 * Close both ends of a pipe.  read_fd and write_fd are set to -1
 * after closing.  Already-closed fds (-1) are silently ignored.
 */
bool proc_pipe_close(ProcPipe *p);

/*
 * Redirect a child's stdin to fd.  Returns false on failure.
 */
bool proc_redirect_stdin(pid_t pid, int fd);

/*
 * Redirect a child's stdout to fd.  Returns false on failure.
 */
bool proc_redirect_stdout(pid_t pid, int fd);

/*
 * Redirect a child's stderr to fd.  Returns false on failure.
 */
bool proc_redirect_stderr(pid_t pid, int fd);

/*
 * Read up to max bytes from a file descriptor.  The result is
 * owned by the supplied allocator.
 *
 * Returns {NULL, 0} if a is NULL or an error occurs.
 */
str_t proc_read_fd(Allocator *a, int fd, size_t max);

/*
 * Write len bytes to a file descriptor.  Returns false on failure.
 */
bool proc_write_fd(int fd, const void *data, size_t len);

/*
 * Write a str_t to a file descriptor.  Returns false on failure.
 */
bool proc_write_str(int fd, str_t s);

/* ── Environment ─────────────────────────────────────────────────── */

/*
 * Get the value of an environment variable.  The result is owned by
 * the supplied allocator.
 *
 * Returns {NULL, 0} if the variable is not found, a is NULL, or
 * allocation fails.
 */
str_t proc_getenv(Allocator *a, str_t key);

/*
 * Set an environment variable.  Returns false on failure; errno is set.
 */
bool proc_setenv(str_t key, str_t value);

/*
 * Remove an environment variable.  Returns false on failure.
 */
bool proc_unsetenv(str_t key);

/*
 * Clear all environment variables.  Returns false on failure.
 */
bool proc_clearenv(void);

/*
 * Take a full snapshot of the current environment.  The array and
 * its elements are owned by the supplied allocator.  *out_count
 * receives the number of entries.
 *
 * Returns NULL on failure; *out_count is set to 0.
 */
str_t *proc_environ(Allocator *a, size_t *out_count);

/* ── Working directory and credentials ───────────────────────────── */

/*
 * Return the current working directory.  The result is owned by
 * the supplied allocator.
 *
 * Returns {NULL, 0} on failure or if a is NULL.
 */
str_t proc_getcwd(Allocator *a);

/*
 * Change the current working directory.  Returns false on failure.
 */
bool proc_chdir(str_t path);

/*
 * Set the real user ID.  Returns false on failure.
 */
bool proc_setuid(uid_t uid);

/*
 * Set the real group ID.  Returns false on failure.
 */
bool proc_setgid(gid_t gid);

/*
 * Set the effective user ID.  Returns false on failure.
 */
bool proc_seteuid(uid_t uid);

/*
 * Set the effective group ID.  Returns false on failure.
 */
bool proc_setegid(gid_t gid);

/* ── Daemon and session ──────────────────────────────────────────── */

/*
 * Daemonize the calling process (classic double-fork + setsid).
 * Returns false on failure; only the parent sees this return value.
 * The child continues execution with a return value of true.
 */
bool proc_daemonize(void);

/*
 * Create a new session.  Returns false on failure.
 */
bool proc_setsid(void);

/*
 * Return the session ID of a process.  Returns -1 on failure.
 */
pid_t proc_getsid(pid_t pid);

/*
 * Return the process group ID of the calling process.
 */
pid_t proc_getpgrp(void);

/*
 * Set the process group ID to the calling process's PID.
 * Returns false on failure.
 */
bool proc_setpgrp(void);

/*
 * Set the process group ID of a process.  Returns false on failure.
 */
bool proc_setpgid(pid_t pid, pid_t pgid);

/* ── Convenience helpers ─────────────────────────────────────────── */

/*
 * Run a command, capture its stdout, and wait for completion.
 * The result string and *out_status are valid.
 *
 * Returns {NULL, 0} on spawn failure; out_status is unchanged.
 */
str_t proc_run_capture(Allocator *a, str_t path,
		       str_t *argv, size_t argc,
		       ProcStatus *out_status);

/*
 * Run a command, discard output, and return success/failure.
 * Returns true if the command exited with code 0.
 */
bool proc_run(str_t path, str_t *argv, size_t argc);

/*
 * Run a shell command and capture stdout (/bin/sh -c).
 * The result string and *out_status are valid.
 *
 * Returns {NULL, 0} on spawn failure.
 */
str_t proc_shell_capture(Allocator *a, str_t command,
			 ProcStatus *out_status);

/*
 * Run a shell command, discard output (/bin/sh -c).
 * Returns true if the command exited with code 0.
 */
bool proc_shell(str_t command);

/*
 * Return true if the process identified by pid is still alive
 * (sends signal 0 to check).
 */
bool proc_is_alive(pid_t pid);

/*
 * Sleep for a number of milliseconds.  May be interrupted by signals.
 */
void proc_sleep_ms(int ms);

/*
 * Sleep for a number of seconds.  May be interrupted by signals.
 */
void proc_sleep_s(int seconds);

#ifdef __cplusplus
}
#endif

#endif
