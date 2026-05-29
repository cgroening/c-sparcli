#include "tty_internal.h"

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>


/* Cursor visibility escapes. */
#define TTY_CURSOR_HIDE "\033[?25l"
#define TTY_CURSOR_SHOW "\033[?25h"


/* ── Module state ────────────────────────────────────────────────────────
 * A single global session is sufficient: prompts are modal and never nest.
 */
static struct termios saved_termios;   /* terminal mode captured at begin   */
static int            tty_fd      = -1; /* /dev/tty (or stdout) descriptor   */
static bool           owns_fd     = false; /* close tty_fd in sc_tty_end?    */
static bool           raw_active  = false;
static bool           atexit_set  = false;

/* Signal handlers we temporarily replace, restored on sc_tty_end. */
static struct sigaction old_int, old_term;


/**
 * Restores the terminal to its saved mode. Idempotent and async-signal
 * safe enough for use from a handler (only `tcsetattr` + `write`).
 */
static void restore_terminal(void) {
    if (!raw_active) { return; }
    tcsetattr(tty_fd, TCSAFLUSH, &saved_termios);
    const char *show = TTY_CURSOR_SHOW;
    ssize_t ignored = write(tty_fd, show, sizeof(TTY_CURSOR_SHOW) - 1);
    (void)ignored;
    raw_active = false;
}

/** atexit hook: guarantees restoration if the caller exits mid-prompt. */
static void atexit_restore(void) {
    restore_terminal();
}

/**
 * Signal handler for SIGINT/SIGTERM: restore the terminal, then re-raise
 * with the default disposition so the process dies as the user expects.
 */
static void signal_restore(int signum) {
    restore_terminal();
    struct sigaction dfl;
    sigemptyset(&dfl.sa_mask);
    dfl.sa_flags   = 0;
    dfl.sa_handler = SIG_DFL;
    sigaction(signum, &dfl, NULL);
    raise(signum);
}


ScInputStatus sc_tty_begin(void) {
    if (raw_active) { return SC_INPUT_OK; }

    /* Prefer the controlling terminal so input still works when stdout is
     * redirected to a pipe/file. Fall back to stdin/stdout when /dev/tty is
     * unavailable (and only then require them to be TTYs). */
    tty_fd  = open("/dev/tty", O_RDWR | O_CLOEXEC);
    owns_fd = (tty_fd >= 0);
    if (tty_fd < 0) {
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
            return SC_INPUT_ERROR;
        }
        tty_fd  = STDIN_FILENO;  /* read fd; writes also target this number */
        owns_fd = false;
    }

    if (tcgetattr(tty_fd, &saved_termios) != 0) {
        if (owns_fd) { close(tty_fd); tty_fd = -1; }
        return SC_INPUT_ERROR;
    }

    struct termios raw = saved_termios;
    /* Canonical off, echo off, signals off (Ctrl-C becomes a byte). */
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= ~(tcflag_t)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cc[VMIN]  = 1;  /* block for at least one byte */
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(tty_fd, TCSAFLUSH, &raw) != 0) {
        if (owns_fd) { close(tty_fd); tty_fd = -1; }
        return SC_INPUT_ERROR;
    }
    raw_active = true;

    /* Install safety nets. */
    if (!atexit_set) { atexit(atexit_restore); atexit_set = true; }
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = signal_restore;
    sigaction(SIGINT,  &sa, &old_int);
    sigaction(SIGTERM, &sa, &old_term);

    sc_tty_puts(TTY_CURSOR_HIDE);
    return SC_INPUT_OK;
}

void sc_tty_end(void) {
    if (!raw_active && tty_fd < 0) { return; }
    restore_terminal();
    sigaction(SIGINT,  &old_int,  NULL);
    sigaction(SIGTERM, &old_term, NULL);
    if (owns_fd && tty_fd >= 0) { close(tty_fd); }
    tty_fd  = -1;
    owns_fd = false;
}

void sc_tty_write(const char *bytes, size_t n) {
    int fd = (tty_fd >= 0) ? tty_fd : STDOUT_FILENO;
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, bytes + off, n - off);
        if (w <= 0) { break; }
        off += (size_t)w;
    }
}

void sc_tty_puts(const char *s) {
    if (!s) { return; }
    size_t n = 0;
    while (s[n]) { n++; }
    sc_tty_write(s, n);
}

int sc_tty_internal_fd(void) {
    return (tty_fd >= 0) ? tty_fd : STDIN_FILENO;
}

bool sc_input_available(void) {
    int fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (fd >= 0) { close(fd); return true; }
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}
