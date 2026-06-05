/*
 * Self-driving test for the sparcli CLI input commands: forks the CLI binary
 * onto a pseudo-terminal, feeds it canned keystrokes, and asserts both the
 * value printed to stdout and the exit code. The child's stdout is detached
 * from the PTY onto a pipe, exactly like shell command substitution
 * (`name=$(sparcli input ...)`), so this verifies the UI-on-tty /
 * value-on-stdout contract end to end.
 *
 *     make test-cli-pty       # runs ./sparcli-sanitize (ASan/UBSan) headless
 *
 * Usage: test_cli_pty <path-to-sparcli-binary>
 */

#if defined(__APPLE__)
#  include <util.h>
#else
#  include <pty.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

enum {
    MAX_ARGS        = 16,
    OUTPUT_CAPACITY = 8192,
    EXIT_EXEC_FAIL  = 127,
};

/* Path to the CLI binary under test (argv[1]). */
static const char *g_cli = NULL;

/** One test case: CLI arguments, keystrokes, expected stdout and exit. */
typedef struct Case {
    const char *name;
    const char *args[MAX_ARGS]; /**< CLI argv tail (NULL-terminated). */
    const char *keys;           /**< Keystrokes fed via the PTY master. */
    const char *stdin_data;     /**< Piped to child stdin (NULL = none). */
    const char *want_stdout;    /**< Expected stdout (NULL = don't check). */
    int         want_exit;      /**< Expected exit code. */
    bool        no_tty;         /**< Run without any terminal (plain fork). */
} Case;

static const Case CASES[] = {
    /* confirm: exit code carries the answer */
    { .name = "confirm-yes", .args = { "confirm", "Proceed?" },
      .keys = "y", .want_stdout = "", .want_exit = 0 },
    { .name = "confirm-no", .args = { "confirm", "Proceed?" },
      .keys = "n", .want_stdout = "", .want_exit = 1 },
    { .name = "confirm-print", .args = { "confirm", "--print", "Proceed?" },
      .keys = "y", .want_stdout = "yes\n", .want_exit = 0 },
    { .name = "confirm-esc-cancels", .args = { "confirm", "Proceed?" },
      .keys = "\x1b", .want_stdout = "", .want_exit = 1 },
    /* Pasted text cannot answer a confirmation (paste-to-confirm attack):
       the pasted "y" is dropped, only the real "n" afterwards counts. */
    { .name = "confirm-ignores-pasted-answer",
      .args = { "confirm", "Proceed?" },
      .keys = "\x1b[200~y\x1b[201~n", .want_stdout = "", .want_exit = 1 },

    /* input / password: value on stdout */
    { .name = "input-value", .args = { "input", "Name:" },
      .keys = "hi\r", .want_stdout = "hi\n", .want_exit = 0 },
    { .name = "input-esc-cancels", .args = { "input", "Name:" },
      .keys = "ab\x1b", .want_stdout = "", .want_exit = 1 },
    { .name = "input-initial", .args = { "input", "--initial", "abc",
                                         "Name:" },
      .keys = "\r", .want_stdout = "abc\n", .want_exit = 0 },
    { .name = "input-markup-prompt", .args = { "input",
                                               "[bold]Name[/]:" },
      .keys = "x\r", .want_stdout = "x\n", .want_exit = 0 },
    { .name = "input-max-chars", .args = { "input", "--max", "3", "Code:" },
      .keys = "abcdef\r", .want_stdout = "abc\n", .want_exit = 0 },
    /* Pasted ANSI sequences are decoded as keys, never inserted as text:
       unrecognized sequences are skipped by the key reader, so the typed
       characters survive and the value can never carry escape codes. */
    { .name = "input-ignores-pasted-ansi", .args = { "input", "Name:" },
      .keys = "\x1b[31mhi\x1b[0m\r", .want_stdout = "hi\n",
      .want_exit = 0 },
    /* Bracketed paste: pasted text is literal - a pasted \r does not
       submit, the characters land in the field, and only the real Enter
       after the paste submits. */
    { .name = "input-paste-is-literal", .args = { "input", "Name:" },
      .keys = "ab\x1b[200~cd\rEF\x1b[201~gh\r",
      .want_stdout = "abcdEFgh\n", .want_exit = 0 },
    { .name = "password-value", .args = { "password", "Secret:" },
      .keys = "hunter2\r", .want_stdout = "hunter2\n", .want_exit = 0 },
    /* New styling/box flags must parse and leave the value/exit intact. */
    { .name = "input-boxed-border-color-style",
      .args = { "input", "--boxed", "--border-color", "magenta",
                "--style", "prompt=bold cyan", "--no-count", "Name:" },
      .keys = "Bob\r", .want_stdout = "Bob\n", .want_exit = 0 },

    /* number: arrows step, exact text out */
    { .name = "number-step", .args = { "number", "--initial", "10",
                                       "--step", "5", "Qty:" },
      .keys = "\x1b[A\x1b[A\r", .want_stdout = "20\n", .want_exit = 0 },
    { .name = "number-decimal-comma",
      .args = { "number", "--decimals", "2", "--decimal-sep", ",",
                "--start-empty", "Amount:" },
      .keys = "7,50\r", .want_stdout = "7.50\n", .want_exit = 0 },

    /* textarea: Enter = newline, Ctrl-D submits */
    { .name = "textarea-multiline", .args = { "textarea", "Notes:" },
      .keys = "a\rb\x04", .want_stdout = "a\nb\n", .want_exit = 0 },
    /* textarea accepts multi-line pastes: pasted newlines stay newlines */
    { .name = "textarea-paste-multiline", .args = { "textarea", "Notes:" },
      .keys = "a\x1b[200~b\rc\x1b[201~\x04",
      .want_stdout = "ab\nc\n", .want_exit = 0 },

    /* select: items from args or stdin, choice on stdout */
    { .name = "select-args", .args = { "select", "alpha", "beta", "gamma" },
      .keys = "\x1b[B\r", .want_stdout = "beta\n", .want_exit = 0 },
    { .name = "select-stdin-items", .args = { "select" },
      .stdin_data = "one\ntwo\nthree\n",
      .keys = "\x1b[B\x1b[B\r", .want_stdout = "three\n", .want_exit = 0 },
    { .name = "select-multi", .args = { "select", "--multi", "a", "b", "c" },
      .keys = " \x1b[B \r", .want_stdout = "a\nb\n", .want_exit = 0 },
    { .name = "select-esc-cancels", .args = { "select", "a", "b" },
      .keys = "\x1b", .want_stdout = "", .want_exit = 1 },
    { .name = "select-accent-flag", .args = { "select", "--accent",
                                              "magenta", "a", "b" },
      .keys = "\r", .want_stdout = "a\n", .want_exit = 0 },
    /* boxed frame + content background + padding still returns the raw value */
    { .name = "select-boxed-box",
      .args = { "select", "--boxed", "--border", "double", "--bg", "blue",
                "--padding", "1", "a", "b" },
      .keys = "\x1b[B\r", .want_stdout = "b\n", .want_exit = 0 },
    /* widget bg + content width (no frame) + min-width still returns value */
    { .name = "select-bg-content-width",
      .args = { "select", "--bg", "blue", "--width", "content",
                "--min-width", "20", "a", "b" },
      .keys = "\x1b[B\r", .want_stdout = "b\n", .want_exit = 0 },
    /* default wraps: Up on the first row jumps to the last */
    { .name = "select-wrap-default",
      .args = { "select", "a", "b", "c" },
      .keys = "\x1b[A\r", .want_stdout = "c\n", .want_exit = 0 },
    /* --no-wrap: Up on the first row stays on the first */
    { .name = "select-no-wrap",
      .args = { "select", "--no-wrap", "a", "b", "c" },
      .keys = "\x1b[A\r", .want_stdout = "a\n", .want_exit = 0 },
    /* --arrow-nav: Right submits like Enter, Left exits with code 3 (back) */
    { .name = "select-marker-style",
      .args = { "select", "--marker", "> ", "--style", "selected=bold green",
                "a", "b" },
      .keys = "\x1b[B\r", .want_stdout = "b\n", .want_exit = 0 },
    { .name = "select-arrow-right", .args = { "select", "--arrow-nav",
                                              "a", "b" },
      .keys = "\x1b[B\x1b[C", .want_stdout = "b\n", .want_exit = 0 },
    { .name = "select-arrow-left-back", .args = { "select", "--arrow-nav",
                                                  "a", "b" },
      .keys = "\x1b[D", .want_stdout = "", .want_exit = 3 },

    /* fuzzy: query narrows, Enter picks the best match */
    { .name = "fuzzy-query", .args = { "fuzzy", "Groceries", "Rent",
                                       "Insurance" },
      .keys = "Ren\r", .want_stdout = "Rent\n", .want_exit = 0 },
    { .name = "fuzzy-stdin-items", .args = { "fuzzy" },
      .stdin_data = "apple\nbanana\ncherry\n",
      .keys = "\r", .want_stdout = "apple\n", .want_exit = 0 },
    /* boxed fuzzy with a content background still substitutes the raw value */
    { .name = "fuzzy-boxed-bg",
      .args = { "fuzzy", "--boxed", "--bg", "magenta", "Groceries", "Rent" },
      .keys = "Ren\r", .want_stdout = "Rent\n", .want_exit = 0 },
    /* fuzzy with bg-extent=text + fixed width still returns the raw value */
    { .name = "fuzzy-bg-extent-text",
      .args = { "fuzzy", "--bg-extent", "text", "--width", "30",
                "Groceries", "Rent" },
      .keys = "Ren\r", .want_stdout = "Rent\n", .want_exit = 0 },
    { .name = "fuzzy-tsv-table", .args = { "fuzzy", "--tsv", "--header-row" },
      .stdin_data = "Tool\tLang\nrich\tPython\ngum\tGo\nsparcli\tC\n",
      .keys = "gum\r", .want_stdout = "gum\n", .want_exit = 0 },
    /* --arrow-nav: Right submits the highlighted match, Left exits 3 (back) */
    { .name = "fuzzy-arrow-right", .args = { "fuzzy", "--arrow-nav",
                                             "alpha", "beta" },
      .keys = "\x1b[C", .want_stdout = "alpha\n", .want_exit = 0 },
    { .name = "fuzzy-arrow-left-back", .args = { "fuzzy", "--arrow-nav",
                                                 "alpha", "beta" },
      .keys = "\x1b[D", .want_stdout = "", .want_exit = 3 },
    /* Right with no matching row must not submit; the prompt stays open until
       Esc cancels it (exit 1). */
    { .name = "fuzzy-arrow-right-no-match", .args = { "fuzzy", "--arrow-nav",
                                                      "alpha", "beta" },
      .keys = "zzz\x1b[C\x1b", .want_stdout = "", .want_exit = 1 },

    /* date: --initial seeds the calendar, Enter confirms */
    { .name = "date-initial", .args = { "date", "--initial", "2030-05-10" },
      .keys = "\r", .want_stdout = "2030-05-10\n", .want_exit = 0 },
    { .name = "date-format", .args = { "date", "--initial", "2030-05-10",
                                       "--format", "%d.%m.%Y" },
      .keys = "\r", .want_stdout = "10.05.2030\n", .want_exit = 0 },

    /* streaming: spin propagates the child's exit code */
    { .name = "spin-success", .args = { "spin", "--", "true" },
      .keys = "", .want_exit = 0, .no_tty = true },
    { .name = "spin-failure", .args = { "spin", "--", "false" },
      .keys = "", .want_exit = 1, .no_tty = true },
    { .name = "spin-passthrough", .args = { "spin", "--", "echo", "hello" },
      .keys = "", .want_stdout = "hello\n", .want_exit = 0, .no_tty = true },
    { .name = "spin-stdin-consumes", .args = { "spin" },
      .stdin_data = "line 1\nline 2\nline 3\n",
      .keys = "", .want_stdout = "", .want_exit = 0, .no_tty = true },

    /* no terminal at all: input commands fail with exit 2 */
    { .name = "input-no-tty-errors", .args = { "input", "Name:" },
      .keys = "", .want_stdout = "", .want_exit = 2, .no_tty = true },
    { .name = "confirm-no-tty-errors", .args = { "confirm", "Sure?" },
      .keys = "", .want_stdout = "", .want_exit = 2, .no_tty = true },
};
#define N_CASES ((int)(sizeof CASES / sizeof CASES[0]))

static int run_case(const Case *cs);
    static pid_t spawn_on_pty(const Case *cs, int *master, int *out_fd,
                              int *in_fd);
    static pid_t spawn_without_tty(const Case *cs, int *out_fd, int *in_fd);
        static void exec_cli(const Case *cs);
    static void feed_keys(int master, const char *keys);
    static int await_child(pid_t pid, int master);
    static void read_pipe(int fd, char *buffer, size_t capacity);
static void msleep(int ms);
static void drain(int fd);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <sparcli-binary>\n", argv[0]);
        return 1;
    }
    g_cli = argv[1];

    printf("\nDriving the sparcli CLI over a pseudo-terminal:\n");
    int passed = 0;
    for (int i = 0; i < N_CASES; i++) {
        passed += run_case(&CASES[i]);
    }
    printf("%d/%d cases passed\n", passed, N_CASES);
    return passed == N_CASES ? 0 : 1;
}

/** Runs one case; returns 1 on pass, 0 on fail. */
static int run_case(const Case *cs) {
    int   master = -1;
    int   out_fd = -1;
    int   in_fd  = -1;
    pid_t pid    = cs->no_tty ? spawn_without_tty(cs, &out_fd, &in_fd)
                              : spawn_on_pty(cs, &master, &out_fd, &in_fd);
    if (pid < 0) {
        printf("  \033[31m✘\033[0m %s (spawn failed)\n", cs->name);
        return 0;
    }

    /* Provide stdin data (select/fuzzy items), then signal EOF. */
    if (in_fd >= 0) {
        if (cs->stdin_data != NULL) {
            ssize_t w = write(in_fd, cs->stdin_data,
                              strlen(cs->stdin_data));
            (void)w;
        }
        close(in_fd);
    }

    if (!cs->no_tty) {
        msleep(200); /* let the child reach raw mode */
        drain(master);
        feed_keys(master, cs->keys);
    }

    int exit_code = await_child(pid, master);
    if (master >= 0) {
        close(master);
    }

    char output[OUTPUT_CAPACITY] = { 0 };
    read_pipe(out_fd, output, sizeof(output));
    close(out_fd);

    bool exit_ok   = (exit_code == cs->want_exit);
    bool stdout_ok = (cs->want_stdout == NULL
                      || strcmp(output, cs->want_stdout) == 0);
    bool pass      = exit_ok && stdout_ok;

    const char *mark = pass ? "\033[32m✔\033[0m" : "\033[31m✘\033[0m";
    printf("  %s %s\n", mark, cs->name);
    if (!exit_ok) {
        printf("      exit: want %d, got %d\n", cs->want_exit, exit_code);
    }
    if (!stdout_ok) {
        printf("      stdout: want %s, got %s\n",
               cs->want_stdout ? cs->want_stdout : "(any)", output);
    }
    return pass ? 1 : 0;
}

/* Forks the CLI onto a fresh PTY (slave = controlling terminal / /dev/tty)
   with stdout - and optionally stdin - redirected to pipes, mirroring
   `value=$(items | sparcli ...)` in a shell. */
static pid_t spawn_on_pty(const Case *cs, int *master, int *out_fd,
                          int *in_fd) {
    int out_pipe[2] = { -1, -1 };
    int in_pipe[2]  = { -1, -1 };
    if (pipe(out_pipe) != 0 || pipe(in_pipe) != 0) {
        return -1;
    }

    struct winsize ws  = { .ws_row = 24, .ws_col = 80 };
    pid_t          pid = forkpty(master, NULL, NULL, &ws);
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        /* Child: stdout -> pipe (the value channel), stdin -> item pipe;
           /dev/tty still resolves to the PTY slave for the widget UI. */
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(in_pipe[0], STDIN_FILENO);
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(in_pipe[0]);
        close(in_pipe[1]);
        exec_cli(cs);
    }

    close(out_pipe[1]);
    close(in_pipe[0]);
    *out_fd = out_pipe[0];
    *in_fd  = in_pipe[1];
    return pid;
}

/* Forks the CLI with no controlling terminal at all (setsid + pipes), for
   the no-TTY error paths and the spin pass-through cases. */
static pid_t spawn_without_tty(const Case *cs, int *out_fd, int *in_fd) {
    int out_pipe[2] = { -1, -1 };
    int in_pipe[2]  = { -1, -1 };
    if (pipe(out_pipe) != 0 || pipe(in_pipe) != 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        setsid(); /* drop the controlling terminal: /dev/tty won't open */
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(in_pipe[0], STDIN_FILENO);
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(in_pipe[0]);
        close(in_pipe[1]);
        exec_cli(cs);
    }

    close(out_pipe[1]);
    close(in_pipe[0]);
    *out_fd = out_pipe[0];
    *in_fd  = in_pipe[1];
    return pid;
}

/* Builds the child argv (binary + case args) and execs it. */
static void exec_cli(const Case *cs) {
    char *child_argv[MAX_ARGS + 2] = { NULL };
    child_argv[0] = (char *)g_cli;

    int count = 1;
    for (int i = 0; cs->args[i] != NULL && i < MAX_ARGS; i++) {
        child_argv[count] = (char *)cs->args[i];
        count++;
    }
    child_argv[count] = NULL;

    execv(g_cli, child_argv);
    fprintf(stderr, "exec %s failed: %s\n", g_cli, strerror(errno));
    _exit(EXIT_EXEC_FAIL);
}

/* Sends keystrokes through the PTY master. Escape sequences are written
   atomically (a real terminal delivers them as one burst); other keys go
   byte by byte with a small delay. */
static void feed_keys(int master, const char *keys) {
    for (const char *key = keys; *key != '\0';) {
        size_t len = 1;
        if ((unsigned char)*key == 0x1b) {
            const char *end = key + 1;
            while (*end != '\0' && (unsigned char)*end != 0x1b) {
                end++;
            }
            len = (size_t)(end - key);
        }
        ssize_t w = write(master, key, len);
        (void)w;
        key += len;
        msleep(60);
        drain(master);
    }
}

/* Waits for the child (draining the PTY so it never blocks on a full
   buffer) and returns its exit code, or -1 on abnormal termination. */
static int await_child(pid_t pid, int master) {
    enum { MAX_WAIT_TICKS = 200, TICK_MS = 20 };

    int status = 0;
    for (int i = 0; i < MAX_WAIT_TICKS; i++) {
        if (waitpid(pid, &status, WNOHANG) == pid) {
            break;
        }
        if (master >= 0) {
            drain(master);
        }
        msleep(TICK_MS);
    }
    if (!WIFEXITED(status) && !WIFSIGNALED(status)) {
        waitpid(pid, &status, 0);
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

/* Reads everything available from `fd` into `buffer` (NUL-terminated). */
static void read_pipe(int fd, char *buffer, size_t capacity) {
    size_t total = 0;
    while (total < capacity - 1) {
        ssize_t n = read(fd, buffer + total, capacity - 1 - total);
        if (n <= 0) {
            break;
        }
        total += (size_t)n;
    }
    buffer[total] = '\0';
}

static void msleep(int ms) {
    enum { MS_PER_SECOND = 1000, NS_PER_MS = 1000000 };
    struct timespec ts = { ms / MS_PER_SECOND,
                           (long)(ms % MS_PER_SECOND) * NS_PER_MS };
    nanosleep(&ts, NULL);
}

/** Reads and discards whatever the child rendered (keeps the PTY drained). */
static void drain(int fd) {
    for (;;) {
        struct timeval tv = { 0, 40000 };
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(fd, &readable);
        if (select(fd + 1, &readable, NULL, NULL, &tv) <= 0) {
            return;
        }
        char buf[4096];
        if (read(fd, buf, sizeof(buf)) <= 0) {
            return;
        }
    }
}
