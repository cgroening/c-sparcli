#include "tty_internal.h"
#include "internal.h"   /* sc_no_tty_override */


/* Cursor visibility escapes. */
#define TTY_CURSOR_HIDE "\033[?25l"
#define TTY_CURSOR_SHOW "\033[?25h"

/* Bracketed paste mode: the terminal wraps pasted text in ESC[200~ / ESC[201~
 * markers so the key reader can treat it as literal text instead of keys.
 * Terminals without support ignore these sequences (graceful degradation). */
#define TTY_PASTE_ON  "\033[?2004h"
#define TTY_PASTE_OFF "\033[?2004l"


#ifdef _WIN32
/* ════════════════════════════════════════════════════════════════════════
 * Windows console backend (Windows 10 1809+ / Windows Terminal).
 *
 * Raw mode = console input mode with line/echo/processed input cleared and
 * ENABLE_VIRTUAL_TERMINAL_INPUT set, so the console emits the same VT byte
 * sequences a POSIX terminal does (the key decoder in key.c is reused as-is).
 * Output mode gets ENABLE_VIRTUAL_TERMINAL_PROCESSING so our escapes render.
 * Cleanup runs from atexit + a console control handler (no POSIX signals).
 * ════════════════════════════════════════════════════════════════════════ */
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <stdlib.h>   /* atexit */

#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
#  ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#    define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#  endif

static HANDLE in_handle  = INVALID_HANDLE_VALUE;
static HANDLE out_handle = INVALID_HANDLE_VALUE;
static DWORD  saved_in_mode  = 0;
static DWORD  saved_out_mode = 0;
static UINT   saved_out_cp   = 0;
static bool   raw_active = false;
static bool   atexit_set = false;
static bool   ctrl_handler_set = false;

static void restore_terminal(void);
static void atexit_restore(void);
static BOOL WINAPI ctrl_handler(DWORD type);

ScInputStatus sc_tty_begin(void) {
    if (raw_active) {
        return SC_INPUT_OK;
    }
    if (sc_no_tty_override()) {
        return SC_INPUT_ERROR;
    }

    in_handle = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    out_handle = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (in_handle == INVALID_HANDLE_VALUE
        || out_handle == INVALID_HANDLE_VALUE) {
        goto fail;
    }
    if (!GetConsoleMode(in_handle, &saved_in_mode)
        || !GetConsoleMode(out_handle, &saved_out_mode)) {
        goto fail;   /* input/output redirected: not a usable console */
    }

    /* Raw input: no line buffering, echo or input processing (so Ctrl-C
     * arrives as the byte 0x03, matching POSIX ISIG-off). VT input makes the
     * console translate keys to escape sequences; WINDOW_INPUT keeps resize
     * events in the queue so the reader can surface SC_KEY_RESIZE. */
    DWORD raw_in = saved_in_mode;
    raw_in &= ~(DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT
                       | ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT
                       | ENABLE_QUICK_EDIT_MODE);
    raw_in |= ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_WINDOW_INPUT
              | ENABLE_EXTENDED_FLAGS;
    if (!SetConsoleMode(in_handle, raw_in)) {
        goto fail;
    }
    SetConsoleMode(out_handle,
        saved_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);   /* best-effort */

    /* UTF-8 output so box-drawing glyphs and multibyte text render. */
    saved_out_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    raw_active = true;
    sc_tty_input_reset();
    if (!atexit_set) {
        atexit(atexit_restore);
        atexit_set = true;
    }
    if (!ctrl_handler_set) {
        SetConsoleCtrlHandler(ctrl_handler, TRUE);
        ctrl_handler_set = true;
    }

    sc_tty_puts(TTY_CURSOR_HIDE);
    sc_tty_puts(TTY_PASTE_ON);
    return SC_INPUT_OK;

fail:
    if (in_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(in_handle);
        in_handle = INVALID_HANDLE_VALUE;
    }
    if (out_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(out_handle);
        out_handle = INVALID_HANDLE_VALUE;
    }
    return SC_INPUT_ERROR;
}

void sc_tty_end(void) {
    if (!raw_active && in_handle == INVALID_HANDLE_VALUE) {
        return;
    }
    restore_terminal();
    if (ctrl_handler_set) {
        SetConsoleCtrlHandler(ctrl_handler, FALSE);
        ctrl_handler_set = false;
    }
    if (in_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(in_handle);
        in_handle = INVALID_HANDLE_VALUE;
    }
    if (out_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(out_handle);
        out_handle = INVALID_HANDLE_VALUE;
    }
}

void sc_tty_write(const char *bytes, size_t len) {
    HANDLE handle = (out_handle != INVALID_HANDLE_VALUE)
        ? out_handle : GetStdHandle(STD_OUTPUT_HANDLE);
    size_t offset = 0;
    while (offset < len) {
        DWORD wrote = 0;
        if (!WriteFile(handle, bytes + offset, (DWORD)(len - offset),
                       &wrote, NULL)) {
            break;
        }
        if (wrote == 0) {
            break;
        }
        offset += wrote;
    }
}

void sc_tty_puts(const char *str) {
    if (!str) {
        return;
    }
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    sc_tty_write(str, len);
}

int sc_tty_internal_fd(void) {
    return -1;   /* not meaningful on Windows; key.c uses sc_tty_input_handle */
}

void *sc_tty_input_handle(void) {
    return (in_handle != INVALID_HANDLE_VALUE) ? in_handle : NULL;
}

int sc_tty_rows(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE handle = (out_handle != INVALID_HANDLE_VALUE)
        ? out_handle : GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE
        && GetConsoleScreenBufferInfo(handle, &info)) {
        int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        if (rows > 0) { return rows; }
    }
    return 24;
}

int sc_tty_cols(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE handle = (out_handle != INVALID_HANDLE_VALUE)
        ? out_handle : GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE
        && GetConsoleScreenBufferInfo(handle, &info)) {
        int cols = info.srWindow.Right - info.srWindow.Left + 1;
        if (cols > 0) { return cols; }
    }
    return 80;
}

bool sc_tty_take_resize(void) {
    /* On Windows a resize surfaces synchronously from sc_tty_read_key as
     * SC_KEY_RESIZE (WINDOW_BUFFER_SIZE_EVENT); there is no async flag. */
    return false;
}

bool sc_input_available(void) {
    if (sc_no_tty_override()) {
        return false;
    }
    HANDLE handle = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD mode;
    bool is_console = GetConsoleMode(handle, &mode) != 0;
    CloseHandle(handle);
    return is_console;
}

/** Restores console modes, output codepage and cursor. Idempotent. */
static void restore_terminal(void) {
    if (!raw_active) {
        return;
    }
    if (out_handle != INVALID_HANDLE_VALUE) {
        DWORD wrote;
        WriteFile(out_handle, TTY_PASTE_OFF,
                  (DWORD)(sizeof(TTY_PASTE_OFF) - 1), &wrote, NULL);
        WriteFile(out_handle, TTY_CURSOR_SHOW,
                  (DWORD)(sizeof(TTY_CURSOR_SHOW) - 1), &wrote, NULL);
        SetConsoleMode(out_handle, saved_out_mode);
    }
    if (saved_out_cp != 0) {
        SetConsoleOutputCP(saved_out_cp);
    }
    if (in_handle != INVALID_HANDLE_VALUE) {
        SetConsoleMode(in_handle, saved_in_mode);
    }
    raw_active = false;
}

/** `atexit` hook: guarantees restoration if the caller exits mid-prompt. */
static void atexit_restore(void) {
    restore_terminal();
}

/**
 * Console control handler: restores the terminal on Ctrl-Break and on the
 * window-close / logoff / shutdown events, then returns FALSE so the default
 * action still runs. (Ctrl-C arrives as input byte 0x03 because
 * ENABLE_PROCESSED_INPUT is cleared, so it does not reach here.)
 */
static BOOL WINAPI ctrl_handler(DWORD type) {
    switch (type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            restore_terminal();
            return FALSE;
        default:
            return FALSE;
    }
}

#else
/* ════════════════════════════════════════════════════════════════════════
 * POSIX terminal backend (termios + /dev/tty + signals).
 * ════════════════════════════════════════════════════════════════════════ */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/*
 * Termination signals after which we restore the terminal, then re-raise with
 * the default disposition so the process still dies as expected - but with a
 * usable terminal instead of a stuck raw mode.
 *
 * We deliberately do NOT trap the crash signals
 * (SIGSEGV/SIGABRT/SIGBUS/SIGFPE): doing so would override handlers that
 * debuggers, crash reporters and
 * sanitizers install, masking their diagnostics. `atexit` covers clean exits.
 */
static const int RESTORE_SIGNALS[] = {
    SIGINT, SIGTERM, SIGHUP, SIGQUIT,
};
#define N_RESTORE ((int)(sizeof RESTORE_SIGNALS / sizeof RESTORE_SIGNALS[0]))


/* ── Module state (single process-wide terminal session) ────────────────── */

static struct termios saved_termios;          /* mode captured at begin */
static int tty_fd = -1;                        /* /dev/tty or stdout fd */
static bool owns_fd = false;                   /* close tty_fd in sc_tty_end? */
static bool atexit_set = false;
static volatile sig_atomic_t raw_active = 0;   /* read from signal handlers */
static volatile sig_atomic_t resize_pending = 0;

static struct sigaction old_actions[N_RESTORE];
static struct sigaction old_winch;
static bool handlers_installed = false;


/* ── Internal helpers (call order) ──────────────────────────────────────── */

static void install_handlers(void);
    static void signal_restore(int signum);
    static void winch_handler(int signum);
static void uninstall_handlers(void);
static void atexit_restore(void);
static void restore_terminal(void);


ScInputStatus sc_tty_begin(void) {
    if (raw_active) {
        return SC_INPUT_OK;
    }
    if (sc_no_tty_override()) {
        return SC_INPUT_ERROR;
    }

    // Prefer the controlling terminal so input still works when stdout is
    // redirected to a pipe/file. Fall back to stdin/stdout when /dev/tty is
    // unavailable (and only then require them to be TTYs).
    tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    owns_fd = (tty_fd >= 0);
    if (tty_fd < 0) {
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
            return SC_INPUT_ERROR;
        }
        tty_fd = STDIN_FILENO;
        owns_fd = false;
    }

    if (tcgetattr(tty_fd, &saved_termios) != 0) {
        if (owns_fd) {
            close(tty_fd);
        }
        tty_fd = -1;
        return SC_INPUT_ERROR;
    }

    struct termios raw = saved_termios;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= ~(tcflag_t)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(tty_fd, TCSAFLUSH, &raw) != 0) {
        if (owns_fd) {
            close(tty_fd);
        }
        tty_fd = -1;
        return SC_INPUT_ERROR;
    }
    raw_active = 1;

    sc_tty_input_reset();   // drop any bytes left from a previous session
    resize_pending = 0;
    if (!atexit_set) {
        atexit(atexit_restore);
        atexit_set = true;
    }
    install_handlers();

    sc_tty_puts(TTY_CURSOR_HIDE);
    sc_tty_puts(TTY_PASTE_ON);
    return SC_INPUT_OK;
}

void sc_tty_end(void) {
    if (!raw_active && tty_fd < 0) {
        return;
    }
    restore_terminal();
    uninstall_handlers();
    if (owns_fd && tty_fd >= 0) {
        close(tty_fd);
    }
    tty_fd = -1;
    owns_fd = false;
}

void sc_tty_write(const char *bytes, size_t len) {
    int fd = (tty_fd >= 0) ? tty_fd : STDOUT_FILENO;
    size_t offset = 0;
    while (offset < len) {
        ssize_t written = write(fd, bytes + offset, len - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;   // interrupted by a signal, not a real error
            }
            break;
        }
        if (written == 0) {
            break;
        }
        offset += (size_t)written;
    }
}

void sc_tty_puts(const char *str) {
    if (!str) {
        return;
    }
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    sc_tty_write(str, len);
}

int sc_tty_internal_fd(void) {
    return (tty_fd >= 0) ? tty_fd : STDIN_FILENO;
}

int sc_tty_rows(void) {
    struct winsize window_size;
    int fd = (tty_fd >= 0) ? tty_fd : STDOUT_FILENO;
    if (ioctl(fd, TIOCGWINSZ, &window_size) == 0 && window_size.ws_row > 0) {
        return (int)window_size.ws_row;
    }
    return 24;
}

int sc_tty_cols(void) {
    struct winsize window_size;
    int fd = (tty_fd >= 0) ? tty_fd : STDOUT_FILENO;
    if (ioctl(fd, TIOCGWINSZ, &window_size) == 0 && window_size.ws_col > 0) {
        return (int)window_size.ws_col;
    }
    return 80;
}

bool sc_tty_take_resize(void) {
    if (!resize_pending) {
        return false;
    }
    resize_pending = 0;
    return true;
}

bool sc_input_available(void) {
    if (sc_no_tty_override()) {
        return false;
    }
    int fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        close(fd);
        return true;
    }
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}


/** Installs the restore + resize signal handlers, saving the previous ones. */
static void install_handlers(void) {
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;   // no SA_RESTART: a blocking read() wakes on a signal
    action.sa_handler = signal_restore;
    for (int i = 0; i < N_RESTORE; i++) {
        sigaction(RESTORE_SIGNALS[i], &action, &old_actions[i]);
    }
    action.sa_handler = winch_handler;
    sigaction(SIGWINCH, &action, &old_winch);
    handlers_installed = true;
}

/** Restores the terminal, then re-raises `signum` with the default handler. */
static void signal_restore(int signum) {
    restore_terminal();
    struct sigaction dfl;
    sigemptyset(&dfl.sa_mask);
    dfl.sa_flags = 0;
    dfl.sa_handler = SIG_DFL;
    sigaction(signum, &dfl, NULL);
    raise(signum);
}

/** SIGWINCH: flag the resize; the prompt loop repaints on the next tick. */
static void winch_handler(int signum) {
    (void)signum;
    resize_pending = 1;
}

/** Restores the signal handlers saved by `install_handlers`. */
static void uninstall_handlers(void) {
    if (!handlers_installed) {
        return;
    }
    for (int i = 0; i < N_RESTORE; i++) {
        sigaction(RESTORE_SIGNALS[i], &old_actions[i], NULL);
    }
    sigaction(SIGWINCH, &old_winch, NULL);
    handlers_installed = false;
}

/** `atexit` hook: guarantees restoration if the caller exits mid-prompt. */
static void atexit_restore(void) {
    restore_terminal();
}

/**
 * Restores the terminal to its saved mode and shows the cursor. Idempotent
 * and async-signal-safe (only `tcsetattr` + `write`), so it is callable from
 * a signal handler.
 */
static void restore_terminal(void) {
    if (!raw_active) {
        return;
    }
    tcsetattr(tty_fd, TCSAFLUSH, &saved_termios);
    // Best-effort writes: nothing useful can be done on failure inside a
    // signal-safe restore path, so both results are deliberately discarded.
    ssize_t cursor_result =
        write(tty_fd, TTY_CURSOR_SHOW, sizeof(TTY_CURSOR_SHOW) - 1);
    ssize_t paste_result =
        write(tty_fd, TTY_PASTE_OFF, sizeof(TTY_PASTE_OFF) - 1);
    (void)cursor_result;
    (void)paste_result;
    raw_active = 0;
}

#endif  /* _WIN32 */
