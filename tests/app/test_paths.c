#include "test_app.h"
#include "sparcli.h"

#include <ftw.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


// Forward declarations indented to reflect call hierarchy
static void check_env_override(const char *sandbox);
static void check_home_fallback(const char *sandbox);
static void check_all_kinds(const char *sandbox);
static void check_invalid_app_names(void);
static void check_path_file(const char *sandbox);
static void check_invalid_relative_paths(void);
static char *make_sandbox(void);
static void remove_tree(const char *path);
    static int remove_entry(
        const char *path, const struct stat *info, int type, struct FTW *walk
    );
static bool dir_exists(const char *path);
static bool str_eq(const char *actual, const char *expected);


/**
 * XDG path resolution: environment overrides, `$HOME` fallbacks,
 * directory creation, and path-traversal rejection. All paths stay
 * inside a temporary sandbox; no real user directories are touched.
 */
void test_paths(void) {
    // Everything below redirects $HOME into the sandbox; restore it after
    const char *real_home = getenv("HOME");
    char *saved_home = real_home ? strdup(real_home) : NULL;

    char *sandbox = make_sandbox();
    if (!sandbox) {
        CHECK(false, "paths: temporary sandbox created");
        free(saved_home);
        return;
    }

    check_env_override(sandbox);
    check_home_fallback(sandbox);
    check_all_kinds(sandbox);
    check_invalid_app_names();
    check_path_file(sandbox);
    check_invalid_relative_paths();

    remove_tree(sandbox);
    free(sandbox);
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_CACHE_HOME");
    unsetenv("XDG_STATE_HOME");
    if (saved_home) {
        setenv("HOME", saved_home, 1);
        free(saved_home);
    } else {
        unsetenv("HOME");
    }
}


/* ── $XDG_* environment override ─────────────────────────────────────────── */

static void check_env_override(const char *sandbox) {
    char xdg_dir[512];
    snprintf(xdg_dir, sizeof xdg_dir, "%s/xdg-config", sandbox);
    setenv("XDG_CONFIG_HOME", xdg_dir, 1);

    char expected[512];
    snprintf(expected, sizeof expected, "%s/myapp", xdg_dir);

    char *path = sc_path_config("myapp");
    CHECK(str_eq(path, expected),
          "paths: $XDG_CONFIG_HOME overrides the default location");
    CHECK(path && dir_exists(path),
          "paths: the application directory is created");
    free(path);

    // Calling it again on the existing directory still succeeds
    path = sc_path_config("myapp");
    CHECK(path != NULL, "paths: existing directory is accepted (EEXIST)");
    free(path);

    // A relative $XDG_* value must be ignored per the XDG spec
    setenv("XDG_CONFIG_HOME", "relative/path", 1);
    char home_fallback[512];
    snprintf(home_fallback, sizeof home_fallback,
             "%s/home/.config/myapp", sandbox);
    path = sc_path_config("myapp");
    CHECK(str_eq(path, home_fallback),
          "paths: relative $XDG_CONFIG_HOME is ignored (falls back to $HOME)");
    free(path);

    unsetenv("XDG_CONFIG_HOME");
}


/* ── $HOME fallback ──────────────────────────────────────────────────────── */

static void check_home_fallback(const char *sandbox) {
    unsetenv("XDG_CONFIG_HOME");

    char expected[512];
    snprintf(expected, sizeof expected, "%s/home/.config/tool", sandbox);

    char *path = sc_path_config("tool");
    CHECK(str_eq(path, expected),
          "paths: unset $XDG_CONFIG_HOME falls back to ~/.config");
    CHECK(path && dir_exists(path),
          "paths: fallback directory chain is created");
    free(path);
}


/* ── All four path kinds ─────────────────────────────────────────────────── */

static void check_all_kinds(const char *sandbox) {
    static const struct {
        ScPathKind kind;
        const char *suffix;
        const char *label;
    } kinds[] = {
        { SC_PATH_CONFIG, "home/.config/app",      "paths: CONFIG maps to ~/.config" },
        { SC_PATH_DATA,   "home/.local/share/app", "paths: DATA maps to ~/.local/share" },
        { SC_PATH_CACHE,  "home/.cache/app",       "paths: CACHE maps to ~/.cache" },
        { SC_PATH_STATE,  "home/.local/state/app", "paths: STATE maps to ~/.local/state" },
    };

    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
        char expected[512];
        snprintf(expected, sizeof expected, "%s/%s", sandbox, kinds[i].suffix);
        char *path = sc_path(kinds[i].kind, "app");
        CHECK(str_eq(path, expected), kinds[i].label);
        free(path);
    }
}


/* ── Invalid application names ───────────────────────────────────────────── */

static void check_invalid_app_names(void) {
    char *path = sc_path_config(NULL);
    CHECK(path == NULL, "paths: NULL appname is rejected");

    path = sc_path_config("");
    CHECK(path == NULL, "paths: empty appname is rejected");

    path = sc_path_config("a/b");
    CHECK(path == NULL, "paths: appname with '/' is rejected");

    path = sc_path_config("..");
    CHECK(path == NULL, "paths: '..' appname is rejected");

    path = sc_path_config(".");
    CHECK(path == NULL, "paths: '.' appname is rejected");

    path = sc_path_config("evil\x01name");
    CHECK(path == NULL, "paths: appname with control bytes is rejected");

    path = sc_path((ScPathKind)99, "app");
    CHECK(path == NULL, "paths: out-of-range path kind is rejected");
}


/* ── sc_path_file ────────────────────────────────────────────────────────── */

static void check_path_file(const char *sandbox) {
    char expected[512];
    snprintf(expected, sizeof expected,
             "%s/home/.local/state/app/logs/run.log", sandbox);

    char *path = sc_path_file(SC_PATH_STATE, "app", "logs/run.log");
    CHECK(str_eq(path, expected),
          "paths: sc_path_file joins app dir + relative path");

    // Parent directories exist, the file itself is not created
    char parent[512];
    snprintf(parent, sizeof parent, "%s/home/.local/state/app/logs", sandbox);
    CHECK(dir_exists(parent), "paths: sc_path_file creates parent directories");
    CHECK(path && access(path, F_OK) != 0,
          "paths: sc_path_file does not create the file itself");
    free(path);

    // A flat filename (no subdirectory) works too
    path = sc_path_file(SC_PATH_CONFIG, "app", "config.toml");
    CHECK(path != NULL, "paths: flat relative filename is accepted");
    free(path);
}


/* ── Invalid relative paths ──────────────────────────────────────────────── */

static void check_invalid_relative_paths(void) {
    char *path = sc_path_file(SC_PATH_CONFIG, "app", NULL);
    CHECK(path == NULL, "paths: NULL relative path is rejected");

    path = sc_path_file(SC_PATH_CONFIG, "app", "");
    CHECK(path == NULL, "paths: empty relative path is rejected");

    path = sc_path_file(SC_PATH_CONFIG, "app", "/etc/passwd");
    CHECK(path == NULL, "paths: absolute relative path is rejected");

    path = sc_path_file(SC_PATH_CONFIG, "app", "../escape");
    CHECK(path == NULL, "paths: leading '..' segment is rejected");

    path = sc_path_file(SC_PATH_CONFIG, "app", "logs/../../escape");
    CHECK(path == NULL, "paths: embedded '..' segment is rejected");

    path = sc_path_file(SC_PATH_CONFIG, "app", "logs//double");
    CHECK(path == NULL, "paths: empty path component ('//') is rejected");
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/**
 * Creates a temporary sandbox directory and points `$HOME` into it, so
 * the fallback paths never touch the real home directory. Returns the
 * heap-allocated sandbox path or `NULL`.
 */
static char *make_sandbox(void) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) { tmp = "/tmp"; }

    size_t need = strlen(tmp) + sizeof("/sparcli-paths-XXXXXX");
    char *sandbox = malloc(need);
    if (!sandbox) { return NULL; }
    snprintf(sandbox, need, "%s/sparcli-paths-XXXXXX", tmp);

    if (!mkdtemp(sandbox)) {
        free(sandbox);
        return NULL;
    }

    // $HOME inside the sandbox: fallback paths stay self-contained
    char home[512];
    snprintf(home, sizeof home, "%s/home", sandbox);
    if (mkdir(home, 0700) != 0) {
        remove_tree(sandbox);
        free(sandbox);
        return NULL;
    }
    setenv("HOME", home, 1);

    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_CACHE_HOME");
    unsetenv("XDG_STATE_HOME");
    return sandbox;
}

/** Recursively removes `path` (best effort, for sandbox cleanup). */
static void remove_tree(const char *path) {
    nftw(path, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
}

/** `nftw` callback: removes one file or (post-order) directory. */
static int remove_entry(
    const char *path, const struct stat *info, int type, struct FTW *walk
) {
    (void)info;
    (void)type;
    (void)walk;
    return remove(path);
}

/** Returns `true` when `path` exists and is a directory. */
static bool dir_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

/** NULL-safe string equality. */
static bool str_eq(const char *actual, const char *expected) {
    return actual != NULL && strcmp(actual, expected) == 0;
}
