```markdown
# std/process.h

<!--toc:start-->
- [std/process.h](#stdprocessh)
  - [Ownership](#ownership)
  - [Core primitives `core/proc.h`](#core-primitives-coreproch)
  - [Process identity `proc_*`](#process-identity-proc)
  - [Spawning and execution](#spawning-and-execution)
  - [Waiting and status](#waiting-and-status)
  - [Signals and termination](#signals-and-termination)
  - [Pipes and redirection](#pipes-and-redirection)
  - [Environment](#environment)
  - [Working directory and credentials](#working-directory-and-credentials)
  - [Daemon and session](#daemon-and-session)
  - [Convenience helpers](#convenience-helpers)
  - [Compatibility with libc](#compatibility-with-libc)
  - [Design goals](#design-goals)
<!--toc:end-->

`process.h` is part of the Quasar standard library. It provides practical, allocator-aware helpers for process management so that programs do not need to repeatedly re-implement the same thin wrappers around POSIX process primitives.

The public API lives in `std/process.h`. Low-level building blocks and platform-specific details live in `core/proc.h`. Higher-level code should normally use only the `std` surface.

All functions that return owned data take an `Allocator*` and return memory owned by it. Boolean-returning operations indicate success or failure; on failure the caller may inspect `errno`.

---

## Ownership

Process handles and status values are plain values. Strings and arrays returned by process helpers are owned by the supplied allocator.

Objects returned by these functions do not own the allocator that created them. Their lifetime is governed by the allocator’s rules (see `std/memory.h`).

File descriptors obtained from pipe helpers remain the caller’s responsibility unless an explicit close helper is used.

---

## Core primitives `core/proc.h`

`core/proc.h` contains the minimal, architecture-aware building blocks used by the public API. These are not intended for everyday use; they exist so that `std/process.h` can remain portable and consistent.

Typical contents:

```c
/* Opaque or thin wrappers around platform process IDs and handles */
typedef /* platform-specific */ ProcId;

/* Low-level spawn / wait / signal primitives */
int  proc_raw_spawn(...);
int  proc_raw_wait(...);
int  proc_raw_kill(...);
```

The public `std` layer never requires callers to touch these symbols.

---

## Process identity `proc_*`

```c
pid_t  proc_getpid(void);
pid_t  proc_getppid(void);
uid_t  proc_getuid(void);
gid_t  proc_getgid(void);
uid_t  proc_geteuid(void);
gid_t  proc_getegid(void);

str_t  proc_name(Allocator *a);                 /* current process name */
str_t  proc_exe(Allocator *a);                  /* path to current executable */
```

These are thin, predictable wrappers. They never allocate unless an `Allocator*` is supplied.

---

## Spawning and execution

```c
typedef struct {
    str_t  path;           /* executable path */
    str_t *argv;           /* NULL-terminated argument vector (owned by caller) */
    size_t argc;
    str_t *envp;           /* optional environment; NULL = inherit */
    size_t envc;
    str_t  cwd;            /* optional working directory; empty = inherit */
    int    stdin_fd;       /* -1 = inherit */
    int    stdout_fd;
    int    stderr_fd;
    bool   search_path;    /* use PATH lookup */
} ProcSpawnOpts;

pid_t  proc_spawn(const ProcSpawnOpts *opts);
pid_t  proc_spawn_simple(str_t path, str_t *argv, size_t argc);

bool   proc_exec(str_t path, str_t *argv, size_t argc);   /* replace current image */
bool   proc_exec_env(str_t path, str_t *argv, size_t argc,
                     str_t *envp, size_t envc);

pid_t  proc_system(Allocator *a, str_t command);          /* shell-style, returns child pid */
```

`proc_spawn` is the general entry point. `proc_spawn_simple` covers the common case of “run this binary with these arguments”. `proc_exec` replaces the current process image (never returns on success).

All path and argument strings are `str_t`; the caller retains ownership of the input arrays.

---

## Waiting and status

```c
typedef struct {
    int    exit_code;      /* meaningful only if exited normally */
    int    term_signal;    /* meaningful only if killed by signal */
    bool   exited;
    bool   signaled;
    bool   stopped;
    bool   continued;
} ProcStatus;

bool   proc_wait(pid_t pid, ProcStatus *out);             /* blocking */
bool   proc_wait_timeout(pid_t pid, ProcStatus *out, int ms);
bool   proc_try_wait(pid_t pid, ProcStatus *out);         /* non-blocking */

bool   proc_wait_any(ProcStatus *out, pid_t *out_pid);    /* wait for any child */
bool   proc_wait_all(void);                               /* wait for all children */

int    proc_exit_code(const ProcStatus *st);              /* -1 if not exited */
bool   proc_succeeded(const ProcStatus *st);              /* exited with 0 */
bool   proc_failed(const ProcStatus *st);
```

Status inspection is explicit. Callers never need to decode raw wait status bits themselves.

---

## Signals and termination

```c
bool   proc_kill(pid_t pid, int sig);
bool   proc_terminate(pid_t pid);                         /* SIGTERM */
bool   proc_kill_force(pid_t pid);                        /* SIGKILL */
bool   proc_interrupt(pid_t pid);                         /* SIGINT */

bool   proc_signal_self(int sig);
bool   proc_raise(int sig);

bool   proc_ignore_signal(int sig);
bool   proc_default_signal(int sig);
bool   proc_block_signal(int sig);
bool   proc_unblock_signal(int sig);
```

Thin wrappers around the usual signal operations. Recursive or process-group variants are intentionally separate so cost is visible:

```c
bool   proc_kill_group(pid_t pgid, int sig);
bool   proc_terminate_group(pid_t pgid);
```

---

## Pipes and redirection

```c
typedef struct {
    int read_fd;
    int write_fd;
} ProcPipe;

bool   proc_pipe(ProcPipe *out);
bool   proc_pipe_close(ProcPipe *p);

bool   proc_redirect_stdin(pid_t pid, int fd);
bool   proc_redirect_stdout(pid_t pid, int fd);
bool   proc_redirect_stderr(pid_t pid, int fd);

str_t  proc_read_fd(Allocator *a, int fd, size_t max);
bool   proc_write_fd(int fd, const void *data, size_t len);
bool   proc_write_str(int fd, str_t s);
```

Pipes are created once and then attached via the spawn options or the redirect helpers. Reading from a pipe returns an allocator-owned `str_t`.

---

## Environment

```c
str_t  proc_getenv(Allocator *a, str_t key);
bool   proc_setenv(str_t key, str_t value);
bool   proc_unsetenv(str_t key);
bool   proc_clearenv(void);

str_t *proc_environ(Allocator *a, size_t *out_count);     /* full environment snapshot */
```

Environment helpers that return strings allocate through the supplied allocator. Mutation helpers affect the current process environment only.

---

## Working directory and credentials

```c
str_t  proc_getcwd(Allocator *a);
bool   proc_chdir(str_t path);

bool   proc_setuid(uid_t uid);
bool   proc_setgid(gid_t gid);
bool   proc_seteuid(uid_t uid);
bool   proc_setegid(gid_t gid);
```

These mirror the classic POSIX calls while accepting and returning Quasar `str_t` values where paths are involved.

---

## Daemon and session

```c
bool   proc_daemonize(void);                              /* classic double-fork + setsid */
bool   proc_setsid(void);
pid_t  proc_getsid(pid_t pid);
pid_t  proc_getpgrp(void);
bool   proc_setpgrp(void);
bool   proc_setpgid(pid_t pid, pid_t pgid);
```

`proc_daemonize` performs the usual “become a background daemon” sequence. Individual session and process-group helpers remain available for finer control.

---

## Convenience helpers

```c
/* Run a command, capture stdout, wait for completion */
str_t  proc_run_capture(Allocator *a, str_t path,
                        str_t *argv, size_t argc,
                        ProcStatus *out_status);

/* Run a command, discard output, return success/failure */
bool   proc_run(str_t path, str_t *argv, size_t argc);

/* Shell-style convenience (uses /bin/sh -c) */
str_t  proc_shell_capture(Allocator *a, str_t command,
                          ProcStatus *out_status);
bool   proc_shell(str_t command);

/* Check whether a process is still alive */
bool   proc_is_alive(pid_t pid);

/* Sleep helpers (process-friendly) */
void   proc_sleep_ms(int ms);
void   proc_sleep_s(int seconds);
```

These cover the most common “just run this and give me the output / exit code” patterns without forcing the caller to assemble pipes and wait status manually.

---

## Compatibility with libc

Quasar process helpers are built on the normal POSIX interfaces (`fork`, `execve`, `waitpid`, `kill`, `pipe`, `getenv`, `setenv`, `chdir`, `setsid`, etc.).

They do not replace those interfaces. Programs may freely mix raw POSIX calls with Quasar helpers, provided ownership rules are respected:

- Memory obtained from a Quasar function must be managed through its allocator.
- File descriptors obtained from `proc_pipe` or raw `pipe` remain the caller’s responsibility.
- Process IDs are ordinary `pid_t` values and may be passed to either API.

Quasar’s goal is to make the common higher-level patterns less repetitive, not to hide the underlying system.

---

## Design goals

The process API is designed around the following goals:

- Provide a consistent, predictable interface for everyday process work.
- Integrate cleanly with Quasar’s allocator and `str_t` model.
- Keep low-level primitives in `core/proc.h` and the usable surface in `std/process.h`.
- Make success/failure obvious with boolean returns while still allowing `errno` inspection.
- Prefer explicit status structures over raw wait-status bit decoding.
- Keep recursive, group, and daemon operations clearly named so cost is visible.
- Avoid forcing every program to re-implement the same thin wrappers around `fork`/`exec`/`wait`.
- Remain close enough to POSIX that the mapping is obvious.
- Keep the surface small enough to learn quickly, yet complete enough that most programs need nothing else for ordinary process management.

The functions above form the initial public surface intended for `std/process.h`. Individual signatures and exact edge-case behaviour will be refined as the implementation lands, but the shape and naming conventions are intended to stay stable.

```
