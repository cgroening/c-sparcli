#include "tty_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/* Cursor visibility escapes. */
#define TTY_CURSOR_HIDE "\033[?25l"
#define TTY_CURSOR_SHOW "\033[?25h"


/* ── Module state ──────────────────────────────────────────────────────── */
static struct termios saved_termios;   /* terminal mode captured at begin   */
static int            tty_fd      = -1; /* /dev/tty (or stdout) descriptor   */
static bool           owns_fd     = false; /* close tty_fd in sc_tty_end?    */
static volatile sig_atomic_t raw_active = 0; /* sig-handler reads this       */
static bool           atexit_set  = false;

static volatile sig_atomic_t resize_pending = 0;

/*
 * Termination signals after which we restore the terminal, then re-raise with
 * the default disposition so the process still dies as expected — but with a
 * usable terminal instead of a stuck raw mode.
 *
 * We deliberately do NOT trap the crash signals (SIGSEGV/SIGABRT/SIGBUS/SIGFPE):
 * doing so would override handlers that debuggers, crash reporters and
 * sanitizers install, masking their diagnostics. `atexit` covers clean exits.
 */
static const int RESTORE_SIGNALS[] = {
    SIGINT, SIGTERM, SIGHUP, SIGQUIT,
};
#define N_RESTORE ((int)(sizeof RESTORE_SIGNALS / sizeof RESTORE_SIGNALS[0]))
static struct sigaction old_actions[N_RESTORE];
static struct sigaction old_winch;
static bool handlers_installed = false;


/**
 * Restores the terminal to its saved mode. Idempotent and async-signal-safe
 * (only `tcsetattr` + `write`), so it is callable from a signal handler.
 */
static void restore_terminal(void) {
    if (!raw_active) { return; }
    tcsetattr(tty_fd, TCSAFLUSH, &saved_termios);
    ssize_t ignored = write(tty_fd, TTY_CURSOR_SHOW, sizeof(TTY_CURSOR_SHOW) - 1);
    (void)ignored;
    raw_active = 0;
}

/** atexit hook: guarantees restoration if the caller exits mid-prompt. */
static void atexit_restore(void) {
    restore_terminal();
}

/** Restores the terminal, then re-raises `signum` with the default handler. */
static void signal_restore(int signum) {
    restore_terminal();
    struct sigaction dfl;
    sigemptyset(&dfl.sa_mask);
    dfl.sa_flags   = 0;
    dfl.sa_handler = SIG_DFL;
    sigaction(signum, &dfl, NULL);
    raise(signum);
}

/** SIGWINCH: just flag the resize; the prompt loop repaints on the next tick. */
static void winch_handler(int signum) {
    (void)signum;
    resize_pending = 1;
}

static void install_handlers(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;   /* no SA_RESTART: blocking read() wakes on a signal */
    sa.sa_handler = signal_restore;
    for (int i = 0; i < N_RESTORE; i++) {
        sigaction(RESTORE_SIGNALS[i], &sa, &old_actions[i]);
    }
    sa.sa_handler = winch_handler;
    sigaction(SIGWINCH, &sa, &old_winch);
    handlers_installed = true;
}

static void uninstall_handlers(void) {
    if (!handlers_installed) { return; }
    for (int i = 0; i < N_RESTORE; i++) {
        sigaction(RESTORE_SIGNALS[i], &old_actions[i], NULL);
    }
    sigaction(SIGWINCH, &old_winch, NULL);
    handlers_installed = false;
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
        tty_fd  = STDIN_FILENO;
        owns_fd = false;
    }

    if (tcgetattr(tty_fd, &saved_termios) != 0) {
        if (owns_fd) { close(tty_fd); }
        tty_fd = -1;
        return SC_INPUT_ERROR;
    }

    struct termios raw = saved_termios;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= ~(tcflag_t)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(tty_fd, TCSAFLUSH, &raw) != 0) {
        if (owns_fd) { close(tty_fd); }
        tty_fd = -1;
        return SC_INPUT_ERROR;
    }
    raw_active = 1;

    sc_tty_input_reset();   /* drop any bytes left from a previous session */
    resize_pending = 0;
    if (!atexit_set) { atexit(atexit_restore); atexit_set = true; }
    install_handlers();

    sc_tty_puts(TTY_CURSOR_HIDE);
    return SC_INPUT_OK;
}

void sc_tty_end(void) {
    if (!raw_active && tty_fd < 0) { return; }
    restore_terminal();
    uninstall_handlers();
    if (owns_fd && tty_fd >= 0) { close(tty_fd); }
    tty_fd  = -1;
    owns_fd = false;
}

void sc_tty_write(const char *bytes, size_t n) {
    int fd = (tty_fd >= 0) ? tty_fd : STDOUT_FILENO;
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, bytes + off, n - off);
        if (w < 0) {
            if (errno == EINTR) { continue; }  /* a signal, not a real error */
            break;
        }
        if (w == 0) { break; }
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

int sc_tty_rows(void) {
    struct winsize ws;
    int fd = (tty_fd >= 0) ? tty_fd : STDOUT_FILENO;
    if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return (int)ws.ws_row;
    }
    return 24;
}

bool sc_tty_take_resize(void) {
    if (!resize_pending) { return false; }
    resize_pending = 0;
    return true;
}

bool sc_input_available(void) {
    int fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (fd >= 0) { close(fd); return true; }
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}
