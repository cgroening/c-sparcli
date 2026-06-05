#include "sparcli.h"
#include "core/sanitize_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


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


/* ── helpers ─────────────────────────────────────────────────────────────── */

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

void sc_proc_result_free(ScProcResult *result) {
    if (!result) { return; }
    free(result->out);
    free(result->err);
    result->out = NULL;
    result->err = NULL;
    result->out_len = 0;
    result->err_len = 0;
}
