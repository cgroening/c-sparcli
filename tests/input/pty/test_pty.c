/*
 * Self-driving interactive test: forks each input widget onto a pseudo-terminal
 * and feeds it a canned keystroke script, asserting the returned value. Unlike
 * the manual interactive suite this needs no human, so it can run the
 * interactive code paths under AddressSanitizer in CI:
 *
 *     make test-input-pty           # functional self-check on a PTY
 *     (built with the sanitizer, it also gives interactive ASan coverage)
 */

#include "sparcli.h"

#if defined(__APPLE__)
#  include <util.h>
#else
#  include <pty.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


/* ── The widget under test, run in the forked child ─────────────────────── */

/** Callback-mode shortcut: counts fires via the user pointer, keeps prompt open. */
static bool count_cb(int id, void *user) {
    (void)id;
    (*(int *)user)++;
    return true;   // keep the prompt open
}

/** Callback-mode shortcut: removes the highlighted select item, stays open. */
static bool remove_cb(int id, void *user) {
    (void)id;
    ScSelect *sl = user;
    sc_select_remove(sl, sc_select_cursor(sl));
    return true;
}

/** Returns 0 when the widget produced the expected value, non-zero otherwise. */
static int child_case(int c) {
    switch (c) {
        case 0: {
            bool b = false;
            ScInputStatus s = sc_confirm("Proceed?", &b, (ScConfirmOpts){ 0 });
            return (s == SC_INPUT_OK && b) ? 0 : 1;
        }
        case 1: {
            char *t = NULL;
            ScInputStatus s = sc_text_input("Name", &t, (ScTextInputOpts){ 0 });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "hi") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 2: {
            double v = 0;
            ScInputStatus s = sc_number_input("Qty", &v,
                (ScNumberOpts){ .initial = 10, .step = 5 });
            return (s == SC_INPUT_OK && v == 20.0) ? 0 : 1;
        }
        case 3: {
            ScSelect *sl = sc_select_new((ScSelectOpts){ 0 });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            sc_select_add(sl, "c");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && idx[0] == 1);
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 4: {
            char *t = NULL;
            ScInputStatus s = sc_textarea("Notes", &t, (ScTextareaOpts){ 0 });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "a\nb") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 5: {
            time_t now = time(NULL);
            struct tm *lt = localtime(&now);
            int want_year = lt ? lt->tm_year + 1 : 0;
            struct tm picked = { 0 };  /* zeroed -> seeds today */
            ScInputStatus s = sc_datepicker(&picked, (ScDatePickerOpts){ 0 });
            return (s == SC_INPUT_OK && picked.tm_year == want_year) ? 0 : 1;
        }
        case 6: {
            /* RETURN-mode shortcut: F2 ends the prompt, reports id 42, and the
               widget still returns the typed value. */
            int fired = -1;
            ScShortcut sc[] = {
                { .chord = sc_key_fn(2), .id = 42, .mode = SC_SHORTCUT_RETURN,
                  .hint_label = "submit" },   /* exercises the hint footer */
            };
            char *t = NULL;
            ScTextInputOpts o = {
                .shortcuts = sc, .n_shortcuts = 1, .out_shortcut_id = &fired,
            };
            ScInputStatus s = sc_text_input("Name", &t, o);
            int ok = (s == SC_INPUT_OK && fired == 42 && t
                      && strcmp(t, "ab") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 7: {
            /* CALLBACK-mode shortcut: Ctrl-R fires twice, keeps the prompt open,
               then Enter selects the (unmoved) first item. */
            int count = 0;
            ScShortcut sc[] = {
                { .chord = sc_key_ctrl('r'), .id = 0,
                  .mode = SC_SHORTCUT_CALLBACK,
                  .on_fire = count_cb, .user = &count, .hint_label = "reload" },
            };
            ScSelect *sl = sc_select_new((ScSelectOpts){
                .shortcuts = sc, .n_shortcuts = 1 });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && count == 2 && idx[0] == 0);
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 8: {
            /* Callback removes the highlighted item live, twice, then Enter
               selects the remaining one. Exercises sc_select_remove + the
               empty-tolerant run tail under ASan. */
            ScShortcut sc[] = {
                { .chord = sc_key_ctrl('x'), .id = 0,
                  .mode = SC_SHORTCUT_CALLBACK,
                  .on_fire = remove_cb, .user = NULL, .hint_label = "delete" },
            };
            ScSelect *sl = sc_select_new((ScSelectOpts){
                .shortcuts = sc, .n_shortcuts = 1 });
            sc[0].user = sl;   // same array the opts pointer references
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            sc_select_add(sl, "c");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && idx[0] == 0);  /* "c" remains at 0 */
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 9: {
            /* Esc must still cancel even with shortcuts configured. */
            ScShortcut sc[] = {
                { .chord = sc_key_fn(2), .id = 1, .mode = SC_SHORTCUT_RETURN },
            };
            ScSelect *sl = sc_select_new((ScSelectOpts){
                .shortcuts = sc, .n_shortcuts = 1 });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            sc_select_free(sl);
            return (s == SC_INPUT_CANCELLED) ? 0 : 1;
        }
        default: return 2;
    }
}


/* ── Parent: drive the PTY ──────────────────────────────────────────────── */

typedef struct Case {
    const char *name;
    const char *keys;
} Case;

static const Case CASES[] = {
    { "confirm",  "y" },
    { "text",     "hi\r" },
    { "number",   "\x1b[A\x1b[A\r" },   /* up, up, enter -> 20 */
    { "select",   "\x1b[B\r" },         /* down, enter -> index 1 */
    { "textarea", "a\rb\x04" },         /* a, newline, b, Ctrl-D -> "a\nb" */
    { "datepicker", "\x1b[6;2~\r" },    /* shift+pgdn (year+1), enter */
    { "shortcut-return",   "ab\x1b[12~" },  /* type "ab", then F2 fires (id 42) */
    { "shortcut-callback", "\x12\x12\r" },  /* Ctrl-R x2 (callback), then enter */
    { "shortcut-remove",   "\x18\x18\r" },  /* Ctrl-X x2 (remove), then enter */
    { "esc-cancels",       "\x1b" },        /* Esc still cancels with shortcuts */
};
#define N_CASES ((int)(sizeof CASES / sizeof CASES[0]))

static void msleep(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/** Reads and discards whatever the child has rendered (keeps the PTY drained). */
static void drain(int fd) {
    for (;;) {
        struct timeval tv = { 0, 40000 };
        fd_set r;
        FD_ZERO(&r);
        FD_SET(fd, &r);
        if (select(fd + 1, &r, NULL, NULL, &tv) <= 0) { return; }
        char buf[4096];
        if (read(fd, buf, sizeof buf) <= 0) { return; }
    }
}

/** Runs one case; returns 1 on pass, 0 on fail. */
static int run_case(const Case *cs, int index) {
    int master;
    struct winsize ws = { .ws_row = 24, .ws_col = 80 };
    pid_t pid = forkpty(&master, NULL, NULL, &ws);
    if (pid < 0) { perror("forkpty"); return 0; }

    if (pid == 0) {
        _exit(child_case(index));  /* child: run the widget on the slave PTY */
    }

    msleep(200);                   /* let the child enter raw mode */
    drain(master);
    /* Send each keystroke atomically: an escape sequence (ESC + trailing bytes
     * up to the next ESC) goes in one write, otherwise a single byte. Real
     * terminals deliver a sequence as one burst; writing it byte-by-byte would
     * let the reader's 25 ms lone-Esc timeout split it. */
    for (const char *k = cs->keys; *k; ) {
        size_t len = 1;
        if ((unsigned char)*k == 0x1b) {
            const char *end = k + 1;
            while (*end && (unsigned char)*end != 0x1b) { end++; }
            len = (size_t)(end - k);
        }
        ssize_t w = write(master, k, len);
        (void)w;
        k += len;
        msleep(60);
        drain(master);
    }

    /* Wait for the child, draining so it never blocks on a full PTY buffer. */
    int status = 0;
    for (int i = 0; i < 100; i++) {
        if (waitpid(pid, &status, WNOHANG) == pid) { break; }
        drain(master);
        msleep(20);
    }
    if (!WIFEXITED(status)) { waitpid(pid, &status, 0); }
    close(master);

    int pass = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    printf("  %s %s\n", pass ? "\033[32m✔\033[0m" : "\033[31m✘\033[0m", cs->name);
    return pass;
}

int main(void) {
    /* The parent needs no controlling terminal – forkpty creates a fresh PTY
     * for each child, which becomes that child's controlling terminal. So this
     * runs headless (CI) as well as from an interactive shell. */
    printf("\nDriving input widgets over a pseudo-terminal:\n");
    int passed = 0;
    for (int i = 0; i < N_CASES; i++) { passed += run_case(&CASES[i], i); }
    printf("%d/%d cases passed\n", passed, N_CASES);
    return passed == N_CASES ? 0 : 1;
}
