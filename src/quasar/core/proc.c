#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <quasar/core/proc.h>

/*
 * proc_raw_spawn — fork + exec with fd redirection, working directory,
 * and optional environment.
 *
 * Returns the child pid on success, or -1 on failure.
 */
ProcId
proc_raw_spawn(const char *path, char *const argv[],
	       char *const envp[], const char *cwd,
	       const int fds[3])
{
	pid_t pid;

	if (!path || !argv) {
		errno = EINVAL;
		return -1;
	}

	pid = fork();
	if (pid < 0)
		return -1;

	if (pid == 0) {
		/* ---- child ---- */

		/* Redirect standard file descriptors. */
		if (fds) {
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
		}

		/* Change working directory if requested. */
		if (cwd && cwd[0] != '\0') {
			if (chdir(cwd) < 0)
				_exit(127);
		}

		/* Execute. */
		if (envp)
			execve(path, argv, envp);
		else
			execv(path, argv);

		/* exec failed */
		_exit(127);
	}

	/* ---- parent ---- */
	return pid;
}

/*
 * proc_raw_wait — wait for a child process.  Thin wrapper around
 * waitpid().  Returns the pid of the child, 0 if WNOHANG and no
 * child ready, or -1 on error.
 */
ProcId
proc_raw_wait(ProcId pid, int *wstatus, int flags)
{
	return waitpid(pid, wstatus, flags);
}

/*
 * proc_raw_kill — send a signal to a process.  Thin wrapper around
 * kill().  Returns 0 on success, -1 on error.
 */
int
proc_raw_kill(ProcId pid, int sig)
{
	return kill(pid, sig);
}
