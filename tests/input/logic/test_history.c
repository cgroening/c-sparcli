#include "test_input.h"
#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// Forward declarations indented to reflect call hierarchy
static void check_add_and_get(void);
static void check_duplicates(void);
static void check_entry_cap(void);
static void check_rejected_lines(void);
static void check_sanitization(void);
static void check_null_safety(void);
static void check_file_round_trip(const char *sandbox);
static void check_save_on_free(const char *sandbox);
static void check_xdg_app_persistence(const char *sandbox);
static char *make_sandbox(void);


/**
 * Input-history storage: add/dedupe/cap semantics, sanitization, and
 * plain-text persistence. All files stay inside a temporary sandbox.
 */
void test_history(void) {
    check_add_and_get();
    check_duplicates();
    check_entry_cap();
    check_rejected_lines();
    check_sanitization();
    check_null_safety();

    char *sandbox = make_sandbox();
    if (!sandbox) {
        CHECK(false, "history: temporary sandbox created");
        return;
    }
    check_file_round_trip(sandbox);
    check_save_on_free(sandbox);
    check_xdg_app_persistence(sandbox);

    // Best-effort cleanup: remove the files created by the checks above
    char path[512];
    snprintf(path, sizeof path, "%s/hist.txt", sandbox);
    unlink(path);
    snprintf(path, sizeof path, "%s/freed.txt", sandbox);
    unlink(path);
    snprintf(path, sizeof path, "%s/state/repltest/history", sandbox);
    unlink(path);
    snprintf(path, sizeof path, "%s/state/repltest", sandbox);
    rmdir(path);
    snprintf(path, sizeof path, "%s/state", sandbox);
    rmdir(path);
    rmdir(sandbox);
    free(sandbox);
    unsetenv("XDG_STATE_HOME");
}


/* ── Add / count / get ───────────────────────────────────────────────────── */

static void check_add_and_get(void) {
    ScHistory *hist = sc_history_new((ScHistoryOpts){ 0 });
    CHECK(hist != NULL && sc_history_count(hist) == 0,
          "history: a new history starts empty");

    sc_history_add(hist, "first");
    sc_history_add(hist, "second");
    sc_history_add(hist, "third");
    CHECK(sc_history_count(hist) == 3, "history: entries accumulate");
    CHECK(strcmp(sc_history_get(hist, 0), "first") == 0
              && strcmp(sc_history_get(hist, 2), "third") == 0,
          "history: index 0 is the oldest entry");
    CHECK(sc_history_get(hist, 3) == NULL,
          "history: out-of-range index returns NULL");

    sc_history_free(hist);
}


/* ── Duplicate handling ──────────────────────────────────────────────────── */

static void check_duplicates(void) {
    // Default: consecutive duplicates collapse
    ScHistory *hist = sc_history_new((ScHistoryOpts){ 0 });
    sc_history_add(hist, "list");
    sc_history_add(hist, "list");
    sc_history_add(hist, "add x");
    sc_history_add(hist, "list");
    CHECK(sc_history_count(hist) == 3,
          "history: consecutive duplicates collapse");
    CHECK(strcmp(sc_history_get(hist, 2), "list") == 0,
          "history: non-consecutive duplicates are kept");
    sc_history_free(hist);

    // keep_duplicates retains them
    hist = sc_history_new((ScHistoryOpts){ .keep_duplicates = true });
    sc_history_add(hist, "list");
    sc_history_add(hist, "list");
    CHECK(sc_history_count(hist) == 2,
          "history: keep_duplicates retains consecutive duplicates");
    sc_history_free(hist);
}


/* ── Entry cap / eviction ────────────────────────────────────────────────── */

static void check_entry_cap(void) {
    ScHistory *hist = sc_history_new((ScHistoryOpts){ .max_entries = 3 });
    sc_history_add(hist, "one");
    sc_history_add(hist, "two");
    sc_history_add(hist, "three");
    sc_history_add(hist, "four");

    CHECK(sc_history_count(hist) == 3,
          "history: the entry cap limits the count");
    CHECK(strcmp(sc_history_get(hist, 0), "two") == 0
              && strcmp(sc_history_get(hist, 2), "four") == 0,
          "history: the oldest entry is evicted past the cap");
    sc_history_free(hist);
}


/* ── Rejected lines ──────────────────────────────────────────────────────── */

static void check_rejected_lines(void) {
    ScHistory *hist = sc_history_new((ScHistoryOpts){ 0 });
    sc_history_add(hist, "");
    sc_history_add(hist, NULL);
    sc_history_add(hist, "multi\nline");
    sc_history_add(hist, "carriage\rreturn");
    CHECK(sc_history_count(hist) == 0,
          "history: empty and multi-line entries are rejected");
    sc_history_free(hist);
}


/* ── Trust boundary ──────────────────────────────────────────────────────── */

static void check_sanitization(void) {
    ScHistory *hist = sc_history_new((ScHistoryOpts){ 0 });
    sc_history_add(hist, "safe \033[31mred\033[0m text");
    CHECK(sc_history_count(hist) == 1
              && strcmp(sc_history_get(hist, 0), "safe red text") == 0,
          "history: ANSI escapes are stripped on add");

    // A line that is nothing but escapes sanitizes to empty = rejected
    sc_history_add(hist, "\033[2J\033[H");
    CHECK(sc_history_count(hist) == 1,
          "history: escape-only lines are rejected");
    sc_history_free(hist);
}


/* ── NULL safety ─────────────────────────────────────────────────────────── */

static void check_null_safety(void) {
    sc_history_add(NULL, "x");
    sc_history_free(NULL);
    CHECK(sc_history_count(NULL) == 0 && sc_history_get(NULL, 0) == NULL,
          "history: NULL handle is safe everywhere");
    CHECK(!sc_history_save(NULL) && !sc_history_load(NULL),
          "history: save/load on NULL return false");

    // In-memory history: save/load have no file to work with
    ScHistory *hist = sc_history_new((ScHistoryOpts){ 0 });
    CHECK(!sc_history_save(hist) && !sc_history_load(hist),
          "history: in-memory history has nothing to save/load");
    sc_history_free(hist);
}


/* ── Explicit-file persistence ───────────────────────────────────────────── */

static void check_file_round_trip(const char *sandbox) {
    char file[512];
    snprintf(file, sizeof file, "%s/hist.txt", sandbox);

    ScHistory *hist = sc_history_new((ScHistoryOpts){ .file = file });

    // Opts-string lifetime invariant: the path was copied by _new
    char real_path[512];
    memcpy(real_path, file, sizeof real_path);
    memset(file, 'X', sizeof file - 1);
    file[sizeof file - 1] = '\0';

    sc_history_add(hist, "add \"Buy milk\"");
    sc_history_add(hist, "list --all");
    CHECK(sc_history_save(hist), "history: save to an explicit file succeeds");
    sc_history_free(hist);
    memcpy(file, real_path, sizeof file);   // restore for the checks below

    // A fresh handle on the same file loads the entries back
    hist = sc_history_new((ScHistoryOpts){ .file = file });
    CHECK(sc_history_count(hist) == 2
              && strcmp(sc_history_get(hist, 0), "add \"Buy milk\"") == 0
              && strcmp(sc_history_get(hist, 1), "list --all") == 0,
          "history: entries round-trip through the file");

    // Reload replaces (not appends) the current entries
    sc_history_add(hist, "extra");
    CHECK(sc_history_load(hist) && sc_history_count(hist) == 2,
          "history: load replaces the in-memory entries");
    sc_history_free(hist);
}


/* ── Auto-save on free ───────────────────────────────────────────────────── */

static void check_save_on_free(const char *sandbox) {
    char file[512];
    snprintf(file, sizeof file, "%s/freed.txt", sandbox);

    ScHistory *hist = sc_history_new((ScHistoryOpts){ .file = file });
    sc_history_add(hist, "persisted on free");
    sc_history_free(hist);   // no explicit save

    hist = sc_history_new((ScHistoryOpts){ .file = file });
    CHECK(sc_history_count(hist) == 1
              && strcmp(sc_history_get(hist, 0), "persisted on free") == 0,
          "history: entries are saved automatically on free");
    sc_history_free(hist);
}


/* ── XDG app persistence ─────────────────────────────────────────────────── */

static void check_xdg_app_persistence(const char *sandbox) {
    char state_home[512];
    snprintf(state_home, sizeof state_home, "%s/state", sandbox);
    setenv("XDG_STATE_HOME", state_home, 1);

    ScHistory *hist = sc_history_new((ScHistoryOpts){ .app = "repltest" });
    sc_history_add(hist, "xdg entry");
    CHECK(sc_history_save(hist),
          "history: XDG app persistence saves under $XDG_STATE_HOME");
    sc_history_free(hist);

    char expected[512];
    snprintf(expected, sizeof expected, "%s/repltest/history", state_home);
    FILE *check = fopen(expected, "r");
    CHECK(check != NULL,
          "history: the file lands in $XDG_STATE_HOME/<app>/history");
    if (check) { fclose(check); }
}


/* ── Sandbox helper ──────────────────────────────────────────────────────── */

/** Creates a unique temporary directory; caller frees the returned path. */
static char *make_sandbox(void) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) { tmp = "/tmp"; }

    char *template = malloc(strlen(tmp) + 32);
    if (!template) { return NULL; }
    sprintf(template, "%s/sparcli-hist-XXXXXX", tmp);

    if (!mkdtemp(template)) {
        free(template);
        return NULL;
    }
    return template;
}
