/*
 * Self-driving PTY regression test for the C++ wrapper's string-returning
 * prompts (text_input / password_input / textarea).
 *
 * These wrappers fill an out-pointer through the C call and then hand it to
 * detail::take(). A previous version wrote `take(out, sc_*(..., &out, ...))`,
 * where the unspecified evaluation order of function arguments let `out` be
 * read (still nullptr) before the call filled it — so a typed value came back
 * as an empty string (and the C-allocated string leaked). A non-TTY unit test
 * can't catch this (the prompts return SC_INPUT_ERROR with out == nullptr), so
 * we drive the real interactive path over a pseudo-terminal and assert the
 * returned std::optional actually carries the typed text.
 *
 * Built under ASan/UBSan via `make test-cpp`.
 */

#include <sparcli.hpp>

#if defined(__APPLE__)
#  include <util.h>
#else
#  include <pty.h>
#endif

#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <optional>
#include <string>

using namespace sparcli;

/* ── The wrapper call under test, run in the forked child ───────────────── */

/* Returns 0 when the wrapper returned the expected value, else non-zero. */
static int child_case(int c) {
    switch (c) {
        case 0: {
            std::optional<std::string> r = text_input("Name");
            return (r && *r == "hi") ? 0 : 1;
        }
        case 1: {
            std::optional<std::string> r = password_input("Password");
            return (r && *r == "secret") ? 0 : 1;
        }
        case 2: {
            std::optional<std::string> r = textarea("Notes");
            return (r && *r == "a\nb") ? 0 : 1;
        }
        case 3: {
            /* RETURN-mode shortcut via the C++ Shortcuts builder: F2 ends
               the prompt, fired() reports the id, and the value still
               comes back. */
            Shortcuts sc;
            sc.on_return(key_fn(2), 42);
            TextInputOpts o{};
            sc.apply(o);
            std::optional<std::string> r = text_input("Name", o);
            return (r && *r == "ab" && sc.fired() == 42) ? 0 : 1;
        }
        default: return 2;
    }
}

/* ── Parent: drive the PTY (mirrors tests/input/pty/test_pty.c) ──────────── */

struct Case { const char* name; const char* keys; };

static const Case CASES[] = {
    { "text_input",     "hi\r" },
    { "password_input", "secret\r" },
    { "textarea",       "a\rb\x04" },   /* a, newline, b, Ctrl-D -> "a\nb" */
    { "shortcut",       "ab\x1b[12~" }, /* type "ab", then F2 fires (id 42) */
};
static const int N_CASES = (int)(sizeof CASES / sizeof CASES[0]);

static void msleep(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, nullptr);
}

/* Reads and discards whatever the child rendered (keeps the PTY drained). */
static void drain(int fd) {
    for (;;) {
        struct timeval tv = { 0, 40000 };
        fd_set r;
        FD_ZERO(&r);
        FD_SET(fd, &r);
        if (select(fd + 1, &r, nullptr, nullptr, &tv) <= 0) { return; }
        char buf[4096];
        if (read(fd, buf, sizeof buf) <= 0) { return; }
    }
}

static int run_case(const Case* cs, int index) {
    int master;
    struct winsize ws = { 24, 80, 0, 0 };
    pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) { perror("forkpty"); return 0; }
    if (pid == 0) {
        _exit(child_case(index)); /* child: run wrapper on the slave PTY */
    }

    msleep(200);                    /* let the child enter raw mode */
    drain(master);
    /* Send escape sequences atomically (see tests/input/pty/test_pty.c):
     * writing byte-by-byte would let the reader's 25 ms lone-Esc timeout
     * split them. */
    for (const char* k = cs->keys; *k; ) {
        size_t len = 1;
        if ((unsigned char)*k == 0x1b) {
            const char* end = k + 1;
            while (*end && (unsigned char)*end != 0x1b) { end++; }
            len = (size_t)(end - k);
        }
        ssize_t w = write(master, k, len);
        (void)w;
        k += len;
        msleep(60);
        drain(master);
    }

    int status = 0;
    for (int i = 0; i < 100; i++) {
        if (waitpid(pid, &status, WNOHANG) == pid) { break; }
        drain(master);
        msleep(20);
    }
    if (!WIFEXITED(status)) { waitpid(pid, &status, 0); }
    close(master);

    int pass = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    printf("  %s %s\n", pass ? "\033[32m✔\033[0m" : "\033[31m✘\033[0m",
           cs->name);
    return pass;
}

int main() {
    printf("\nDriving the C++ string prompts over a pseudo-terminal:\n");
    int passed = 0;
    for (int i = 0; i < N_CASES; i++) { passed += run_case(&CASES[i], i); }
    printf("%d/%d cases passed\n", passed, N_CASES);
    return passed == N_CASES ? 0 : 1;
}
