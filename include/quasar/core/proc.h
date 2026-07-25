#ifndef QUASAR_CORE_PROC_H
#define QUASAR_CORE_PROC_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Quasar core process primitives.
 *
 * These are the minimal, platform-aware building blocks used by
 * std/process.h.  They are not intended for everyday use; the std
 * layer provides the safe, allocator-aware surface.
 */

/*
 * Process identifier.  On POSIX systems this is simply pid_t.
 * The typedef exists so that platform-specific implementations
 * can substitute a different type when necessary.
 */
typedef pid_t ProcId;

/*
 * Spawn a child process via fork+exec.  Returns the child pid on
 * success, or -1 on failure (errno is set).
 *
 * path:  executable path (null-terminated C string)
 * argv:  NULL-terminated argument vector
 * envp:  NULL-terminated environment vector, or NULL to inherit
 * cwd:   working directory for the child, or NULL to inherit
 * fds:   {stdin, stdout, stderr} file descriptors, or -1 to inherit
 *
 * This is a low-level primitive.  Higher-level code should use
 * proc_spawn() from std/process.h.
 */
ProcId proc_raw_spawn(const char *path, char *const argv[],
		      char *const envp[], const char *cwd,
		      const int fds[3]);

/*
 * Wait for a child process to change state.  Wraps waitpid().
 *
 * pid:   child process ID
 * wstatus: pointer to int to receive the raw wait status (see
 *          waitpid(2) for decoding)
 * flags:  waitpid flags (0, WNOHANG, etc.)
 *
 * Returns the pid of the child that changed state, 0 if WNOHANG
 * and no child has changed state, or -1 on error (errno is set).
 */
ProcId proc_raw_wait(ProcId pid, int *wstatus, int flags);

/*
 * Send a signal to a process.  Thin wrapper around kill(2).
 *
 * Returns 0 on success, -1 on error (errno is set).
 */
int proc_raw_kill(ProcId pid, int sig);

#ifdef __cplusplus
}
#endif

#endif
