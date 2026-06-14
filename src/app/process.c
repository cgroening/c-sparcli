#include "sparcli.h"
#include "core/sanitize_internal.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <errno.h>
#  include <fcntl.h>
#  include <poll.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif


/* Read chunk size for draining the child's stdout/stderr pipes. */
#define PROC_CHUNK 4096


/* ── growable byte buffer ────────────────────────────────────────────────── */

typedef struct { char *data; size_t len; size_t cap; } Buf;

/** Appends `n` bytes; returns false (and leaves the buffer usable) on OOM. */
static bool buf_append(Buf *b, const char *p, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t ncap = b->cap ? b->cap : PROC_CHUNK;
        while (b->len + n + 1 > ncap) { ncap *= 2; }
        char *grown = realloc(b->data, ncap);
        if (!grown) { return false; }
        b->data = grown;
        b->cap = ncap;
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
    return true;
}

/** Returns a NUL-terminated heap string from the buffer (caller frees). */
static char *buf_finish(Buf *b, size_t *out_len) {
    if (!b->data) {
        char *empty = malloc(1);
        if (empty) { empty[0] = '\0'; }
        *out_len = 0;
        return empty;
    }
    b->data[b->len] = '\0';
    *out_len = b->len;
    return b->data;
}

/**
 * Finalizes a capture buffer into a heap string: verbatim when `raw`,
 * otherwise sanitized at the trust boundary (ANSI/control bytes removed).
 * Consumes the buffer's data.
 */
static char *finish_capture(Buf *b, bool raw, size_t *out_len) {
    char *finished = buf_finish(b, out_len);
    if (raw || !finished) { return finished; }
    char *clean = sc_sanitize_copy(finished, false);
    free(finished);
    if (clean) { *out_len = strlen(clean); }
    return clean;
}


#ifdef _WIN32
/* ════════════════════════════════════════════════════════════════════════
 * Windows backend: CreatePipe + CreateProcessW + I/O threads (no fork/poll).
 *
 * Three pipes (stdin/stdout/stderr) connect to the child; a writer thread
 * feeds stdin and one reader thread per output drains it, so a large input and
 * a chatty child cannot deadlock - the same guarantee poll() gives on POSIX.
 * ════════════════════════════════════════════════════════════════════════ */

/** UTF-8 -> UTF-16 (heap, caller frees). NULL on failure / NULL input. */
static wchar_t *utf8_to_wide(const char *s) {
    if (!s) { return NULL; }
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) { return NULL; }
    wchar_t *w = malloc((size_t)n * sizeof(wchar_t));
    if (!w) { return NULL; }
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

/**
 * Joins argv into a single command line quoted per the CommandLineToArgvW
 * rules (CreateProcess has no argv form). Returns a NUL-terminated heap
 * string, or NULL on OOM.
 */
static char *build_command_line(const char *const *argv) {
    Buf b = { 0 };
    for (size_t i = 0; argv[i]; i++) {
        if (i > 0) { buf_append(&b, " ", 1); }
        const char *a = argv[i];
        bool need_quote = (a[0] == '\0');
        for (const char *p = a; *p && !need_quote; p++) {
            if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\v'
                || *p == '"') {
                need_quote = true;
            }
        }
        if (!need_quote) {
            buf_append(&b, a, strlen(a));
            continue;
        }
        buf_append(&b, "\"", 1);
        size_t slashes = 0;
        for (const char *p = a;; p++) {
            if (*p == '\\') { slashes++; continue; }
            if (*p == '\0') {
                for (size_t k = 0; k < slashes * 2; k++) { buf_append(&b, "\\", 1); }
                break;
            }
            if (*p == '"') {
                for (size_t k = 0; k < slashes * 2 + 1; k++) { buf_append(&b, "\\", 1); }
                buf_append(&b, "\"", 1);
            } else {
                for (size_t k = 0; k < slashes; k++) { buf_append(&b, "\\", 1); }
                buf_append(&b, p, 1);
            }
            slashes = 0;
        }
        buf_append(&b, "\"", 1);
    }
    size_t out_len;
    return buf_finish(&b, &out_len);
}

typedef struct { HANDLE pipe; const char *data; size_t len; } WriterArg;
typedef struct { HANDLE pipe; Buf *buf; } ReaderArg;

/** Feeds `data` to the child's stdin pipe, then closes it (signals EOF). */
static DWORD WINAPI writer_thread(LPVOID param) {
    WriterArg *arg = param;
    size_t sent = 0;
    while (sent < arg->len) {
        DWORD wrote = 0;
        if (!WriteFile(arg->pipe, arg->data + sent,
                       (DWORD)(arg->len - sent), &wrote, NULL) || wrote == 0) {
            break;   /* child closed stdin early (broken pipe): stop */
        }
        sent += wrote;
    }
    CloseHandle(arg->pipe);
    return 0;
}

/** Drains one output pipe into its buffer until the child closes it. */
static DWORD WINAPI reader_thread(LPVOID param) {
    ReaderArg *arg = param;
    char chunk[PROC_CHUNK];
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(arg->pipe, chunk, sizeof chunk, &got, NULL) || got == 0) {
            break;
        }
        buf_append(arg->buf, chunk, got);
    }
    return 0;
}

ScProcResult sc_run(const char *const *argv, ScProcOpts opts) {
    ScProcResult result = { 0 };
    result.exit_code = -1;
    if (!argv || !argv[0]) { return result; }

    size_t input_len = opts.input_len;
    if (opts.input && input_len == 0) { input_len = strlen(opts.input); }
    bool need_err = !opts.merge_stderr;

    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };   /* inheritable */
    HANDLE in_r = NULL, in_w = NULL, out_r = NULL, out_w = NULL;
    HANDLE err_r = NULL, err_w = NULL;
    wchar_t *wcmd = NULL, *wcwd = NULL;

    if (!CreatePipe(&in_r, &in_w, &sa, 0)
        || !CreatePipe(&out_r, &out_w, &sa, 0)
        || (need_err && !CreatePipe(&err_r, &err_w, &sa, 0))) {
        goto cleanup;
    }
    /* Parent-side ends must not leak into the child. */
    SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    if (need_err) { SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0); }

    char *cmdline = build_command_line(argv);
    wcmd = utf8_to_wide(cmdline ? cmdline : "");
    free(cmdline);
    if (opts.cwd) {
        wcwd = utf8_to_wide(opts.cwd);
        if (!wcwd) { goto cleanup; }
    }
    if (!wcmd) { goto cleanup; }

    STARTUPINFOW si = { 0 };
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_r;
    si.hStdOutput = out_w;
    si.hStdError = opts.merge_stderr ? out_w : err_w;
    PROCESS_INFORMATION pi = { 0 };

    if (!CreateProcessW(NULL, wcmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, wcwd, &si, &pi)) {
        goto cleanup;
    }
    free(wcmd); wcmd = NULL;
    free(wcwd); wcwd = NULL;

    /* Close the child's ends in the parent so EOF propagates correctly. */
    CloseHandle(in_r);  in_r = NULL;
    CloseHandle(out_w); out_w = NULL;
    if (need_err) { CloseHandle(err_w); err_w = NULL; }

    Buf out_buf = { 0 };
    Buf err_buf = { 0 };
    HANDLE wt = NULL, ot = NULL, et = NULL;
    WriterArg wa = { in_w, opts.input, input_len };
    if (opts.input && input_len > 0) {
        wt = CreateThread(NULL, 0, writer_thread, &wa, 0, NULL);
        if (!wt) { CloseHandle(in_w); }
        in_w = NULL;   /* owned by the writer thread (or already closed) */
    } else {
        CloseHandle(in_w); in_w = NULL;   /* no input: child sees EOF at once */
    }
    ReaderArg oa = { out_r, &out_buf };
    ot = CreateThread(NULL, 0, reader_thread, &oa, 0, NULL);
    ReaderArg ea = { err_r, &err_buf };
    if (need_err) { et = CreateThread(NULL, 0, reader_thread, &ea, 0, NULL); }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);

    if (wt) { WaitForSingleObject(wt, INFINITE); CloseHandle(wt); }
    if (ot) { WaitForSingleObject(ot, INFINITE); CloseHandle(ot); }
    if (et) { WaitForSingleObject(et, INFINITE); CloseHandle(et); }
    CloseHandle(out_r); out_r = NULL;
    if (need_err) { CloseHandle(err_r); err_r = NULL; }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    result.ok = true;
    result.exit_code = (int)code;
    result.out = finish_capture(&out_buf, opts.raw, &result.out_len);
    if (need_err) {
        result.err = finish_capture(&err_buf, opts.raw, &result.err_len);
    } else {
        free(err_buf.data);
    }
    return result;

cleanup:
    free(wcmd);
    free(wcwd);
    if (in_r)  { CloseHandle(in_r); }
    if (in_w)  { CloseHandle(in_w); }
    if (out_r) { CloseHandle(out_r); }
    if (out_w) { CloseHandle(out_w); }
    if (err_r) { CloseHandle(err_r); }
    if (err_w) { CloseHandle(err_w); }
    return result;
}

#else
/* ════════════════════════════════════════════════════════════════════════
 * POSIX backend: fork + pipe + poll + waitpid.
 * ════════════════════════════════════════════════════════════════════════ */

/** Sets O_NONBLOCK on `fd`. */
static void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) { fcntl(fd, F_SETFL, flags | O_NONBLOCK); }
}

/** Closes `*fd` if open and sets it to -1. */
static void close_fd(int *fd) {
    if (*fd >= 0) { close(*fd); *fd = -1; }
}


/* ── child half ──────────────────────────────────────────────────────────── */

/**
 * Wires the pipe fds to stdio, applies `cwd`, and execs `argv`. Never
 * returns: exits 127 on any failure (the execvp "command not found"
 * convention).
 */
static void exec_child(const char *const *argv, const ScProcOpts *opts,
                       int in_r, int out_w, int err_w) {
    if (opts->cwd && chdir(opts->cwd) != 0) { _exit(127); }

    dup2(in_r, STDIN_FILENO);
    dup2(out_w, STDOUT_FILENO);
    dup2(opts->merge_stderr ? out_w : err_w, STDERR_FILENO);

    /* Close every inherited pipe fd above the std descriptors. */
    int fds[] = { in_r, out_w, err_w };
    for (size_t i = 0; i < sizeof fds / sizeof fds[0]; i++) {
        if (fds[i] > STDERR_FILENO) { close(fds[i]); }
    }

    execvp(argv[0], (char *const *)argv);
    _exit(127);
}


/* ── parent I/O loop ─────────────────────────────────────────────────────── */

/**
 * Feeds `input` to `in_w` while draining `out_r`/`err_r` into `out`/`err`,
 * using `poll` so a large stdin and a chatty child cannot deadlock. Closes
 * the three fds. SIGPIPE is expected to be ignored by the caller so a child
 * that closes stdin early surfaces as `EPIPE`, not a signal.
 */
static void pump_io(int in_w, int out_r, int err_r,
                    const char *input, size_t input_len,
                    Buf *out, Buf *err) {
    size_t sent = 0;
    if (in_w >= 0 && (!input || input_len == 0)) { close_fd(&in_w); }

    while (in_w >= 0 || out_r >= 0 || err_r >= 0) {
        struct pollfd pfds[3];
        int slot_in = -1, slot_out = -1, slot_err = -1;
        nfds_t n = 0;
        if (in_w >= 0) {
            pfds[n].fd = in_w; pfds[n].events = POLLOUT;
            slot_in = (int)n; n++;
        }
        if (out_r >= 0) {
            pfds[n].fd = out_r; pfds[n].events = POLLIN;
            slot_out = (int)n; n++;
        }
        if (err_r >= 0) {
            pfds[n].fd = err_r; pfds[n].events = POLLIN;
            slot_err = (int)n; n++;
        }

        if (poll(pfds, n, -1) < 0) {
            if (errno == EINTR) { continue; }
            break;
        }

        if (slot_in >= 0 && (pfds[slot_in].revents & (POLLOUT | POLLERR
                                                      | POLLHUP))) {
            ssize_t w = write(in_w, input + sent, input_len - sent);
            if (w > 0) {
                sent += (size_t)w;
                if (sent >= input_len) { close_fd(&in_w); }
            } else if (w < 0 && errno != EINTR && errno != EAGAIN) {
                close_fd(&in_w);   /* EPIPE: child stopped reading stdin */
            }
        }

        char chunk[PROC_CHUNK];
        if (slot_out >= 0 && (pfds[slot_out].revents & (POLLIN | POLLHUP
                                                        | POLLERR))) {
            ssize_t r = read(out_r, chunk, sizeof chunk);
            if (r > 0) { buf_append(out, chunk, (size_t)r); }
            else if (r == 0 || (errno != EINTR && errno != EAGAIN)) {
                close_fd(&out_r);
            }
        }
        if (slot_err >= 0 && (pfds[slot_err].revents & (POLLIN | POLLHUP
                                                        | POLLERR))) {
            ssize_t r = read(err_r, chunk, sizeof chunk);
            if (r > 0) { buf_append(err, chunk, (size_t)r); }
            else if (r == 0 || (errno != EINTR && errno != EAGAIN)) {
                close_fd(&err_r);
            }
        }
    }

    close_fd(&in_w);
    close_fd(&out_r);
    close_fd(&err_r);
}


/* ── public API ──────────────────────────────────────────────────────────── */

ScProcResult sc_run(const char *const *argv, ScProcOpts opts) {
    ScProcResult result = { 0 };
    result.exit_code = -1;
    if (!argv || !argv[0]) { return result; }

    size_t input_len = opts.input_len;
    if (opts.input && input_len == 0) { input_len = strlen(opts.input); }

    int in_pipe[2]  = { -1, -1 };
    int out_pipe[2] = { -1, -1 };
    int err_pipe[2] = { -1, -1 };
    bool need_err = !opts.merge_stderr;

    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0
        || (need_err && pipe(err_pipe) != 0)) {
        close_fd(&in_pipe[0]);  close_fd(&in_pipe[1]);
        close_fd(&out_pipe[0]); close_fd(&out_pipe[1]);
        close_fd(&err_pipe[0]); close_fd(&err_pipe[1]);
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close_fd(&in_pipe[0]);  close_fd(&in_pipe[1]);
        close_fd(&out_pipe[0]); close_fd(&out_pipe[1]);
        close_fd(&err_pipe[0]); close_fd(&err_pipe[1]);
        return result;
    }

    if (pid == 0) {
        /* Child: close parent ends, wire pipes, exec. */
        close(in_pipe[1]);
        close(out_pipe[0]);
        if (need_err) { close(err_pipe[0]); }
        exec_child(argv, &opts, in_pipe[0], out_pipe[1],
                   need_err ? err_pipe[1] : out_pipe[1]);
    }

    /* Parent: close child ends, drain pipes, reap. */
    close_fd(&in_pipe[0]);
    close_fd(&out_pipe[1]);
    if (need_err) { close_fd(&err_pipe[1]); }

    set_nonblock(in_pipe[1]);
    set_nonblock(out_pipe[0]);
    if (need_err) { set_nonblock(err_pipe[0]); }

    /* A child that closes stdin early would otherwise SIGPIPE the writer. */
    void (*old_pipe)(int) = signal(SIGPIPE, SIG_IGN);

    Buf out_buf = { 0 };
    Buf err_buf = { 0 };
    pump_io(in_pipe[1], out_pipe[0], need_err ? err_pipe[0] : -1,
            opts.input, input_len, &out_buf, &err_buf);

    signal(SIGPIPE, old_pipe);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) { break; }
    }

    result.ok = true;
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = -1;
        result.term_signal = WTERMSIG(status);
    }

    result.out = finish_capture(&out_buf, opts.raw, &result.out_len);
    if (need_err) {
        result.err = finish_capture(&err_buf, opts.raw, &result.err_len);
    } else {
        free(err_buf.data);
    }

    return result;
}

#endif  /* _WIN32 */


void sc_proc_result_free(ScProcResult *result) {
    if (!result) { return; }
    free(result->out);
    free(result->err);
    result->out = NULL;
    result->err = NULL;
    result->out_len = 0;
    result->err_len = 0;
}
