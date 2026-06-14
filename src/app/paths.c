#include "sparcli.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#  include <direct.h>
#  define mkdir(path, mode) _mkdir(path)   /* Win _mkdir takes no mode */
#else
#  include <sys/stat.h>
#endif


/** Directory mode for created path components (user-private). */
#define PATH_DIR_MODE 0700

/* Path-separator predicate and the home environment variable differ by OS. */
#ifdef _WIN32
#  define SC_PATH_HOME_ENV "USERPROFILE"
#  define SC_IS_SEP(c) ((c) == '/' || (c) == '\\')
#else
#  define SC_PATH_HOME_ENV "HOME"
#  define SC_IS_SEP(c) ((c) == '/')
#endif

/** Returns true for an absolute path: `/foo` everywhere, plus `C:\...` / `\\`
 *  (UNC) / `\foo` on Windows. */
static bool path_is_absolute(const char *p) {
    if (!p || !p[0]) { return false; }
#ifdef _WIN32
    if (SC_IS_SEP(p[0])) { return true; }
    bool drive_letter = (p[0] >= 'A' && p[0] <= 'Z')
                     || (p[0] >= 'a' && p[0] <= 'z');
    return drive_letter && p[1] == ':';
#else
    return p[0] == '/';
#endif
}

/** Number of entries in `PATH_SPECS` (mirrors `ScPathKind`). */
#define PATH_KIND_COUNT 4


/** Environment variable + `$HOME` fallback for one `ScPathKind`. */
typedef struct PathSpec {
    const char *env_var;        /**< XDG environment variable. */
    const char *home_fallback;  /**< Path under `$HOME` when unset. */
} PathSpec;

/** XDG base-directory table, indexed by `ScPathKind`. */
static const PathSpec PATH_SPECS[PATH_KIND_COUNT] = {
    [SC_PATH_CONFIG] = { "XDG_CONFIG_HOME", ".config"       },
    [SC_PATH_DATA]   = { "XDG_DATA_HOME",   ".local/share"  },
    [SC_PATH_CACHE]  = { "XDG_CACHE_HOME",  ".cache"        },
    [SC_PATH_STATE]  = { "XDG_STATE_HOME",  ".local/state"  },
};


// Forward declarations indented to reflect call hierarchy
static bool is_valid_app_name(const char *appname);
static bool is_valid_relative_path(const char *relative);
    static bool is_safe_path_component(const char *start, size_t length);
static char *resolve_base_dir(ScPathKind kind);
    static char *join_paths(const char *left, const char *right);
static bool make_dirs(const char *path);


char *sc_path(ScPathKind kind, const char *appname) {
    if (kind < 0 || kind >= PATH_KIND_COUNT) { return NULL; }
    if (!is_valid_app_name(appname)) { return NULL; }

    char *base = resolve_base_dir(kind);
    if (!base) { return NULL; }

    char *app_dir = join_paths(base, appname);
    free(base);
    if (!app_dir) { return NULL; }

    if (!make_dirs(app_dir)) {
        free(app_dir);
        return NULL;
    }
    return app_dir;
}

char *sc_path_config(const char *appname) {
    return sc_path(SC_PATH_CONFIG, appname);
}

char *sc_path_data(const char *appname) {
    return sc_path(SC_PATH_DATA, appname);
}

char *sc_path_cache(const char *appname) {
    return sc_path(SC_PATH_CACHE, appname);
}

char *sc_path_state(const char *appname) {
    return sc_path(SC_PATH_STATE, appname);
}

char *sc_path_file(
    ScPathKind kind, const char *appname, const char *relative
) {
    if (!is_valid_relative_path(relative)) { return NULL; }

    char *app_dir = sc_path(kind, appname);
    if (!app_dir) { return NULL; }

    char *file_path = join_paths(app_dir, relative);
    free(app_dir);
    if (!file_path) { return NULL; }

    // Create the parent directories of the file (not the file itself)
    char *last_separator = strrchr(file_path, '/');
    if (last_separator && last_separator != file_path) {
        *last_separator = '\0';
        bool parents_ok = make_dirs(file_path);
        *last_separator = '/';
        if (!parents_ok) {
            free(file_path);
            return NULL;
        }
    }
    return file_path;
}

/**
 * Returns `true` when `appname` is a single, safe path component:
 * non-empty, no `/`, not `.`/`..`, no control bytes.
 */
static bool is_valid_app_name(const char *appname) {
    if (!appname || appname[0] == '\0') { return false; }
    return is_safe_path_component(appname, strlen(appname));
}

/**
 * Returns `true` when `relative` is a safe path below the application
 * directory: not empty, not absolute, and every `/`-separated component
 * is safe (no `..`, `.`, empty components or control bytes).
 */
static bool is_valid_relative_path(const char *relative) {
    if (!relative || relative[0] == '\0' || relative[0] == '/') {
        return false;
    }

    const char *component_start = relative;
    for (const char *cursor = relative; ; cursor++) {
        if (*cursor != '/' && *cursor != '\0') { continue; }
        size_t length = (size_t)(cursor - component_start);
        if (!is_safe_path_component(component_start, length)) {
            return false;
        }
        if (*cursor == '\0') { break; }
        component_start = cursor + 1;
    }
    return true;
}

/**
 * Returns `true` when the path component `[start, start+length)` is
 * non-empty, not `.` or `..`, and free of `/` and control bytes.
 */
static bool is_safe_path_component(const char *start, size_t length) {
    if (length == 0) { return false; }
    if (length == 1 && start[0] == '.') { return false; }
    if (length == 2 && start[0] == '.' && start[1] == '.') { return false; }

    for (size_t i = 0; i < length; i++) {
        unsigned char byte = (unsigned char)start[i];
        if (byte == '/' || byte < 0x20 || byte == 0x7f) { return false; }
    }
    return true;
}

/**
 * Resolves the XDG base directory for `kind`: the environment variable
 * when it holds an absolute path, otherwise `$HOME` + the fallback.
 * Returns a heap string or `NULL` when no usable base exists.
 */
static char *resolve_base_dir(ScPathKind kind) {
    const PathSpec *spec = &PATH_SPECS[kind];

    // Per the XDG spec, relative values in $XDG_* must be ignored
    const char *env_value = getenv(spec->env_var);
    if (env_value && path_is_absolute(env_value)) {
        return strdup(env_value);
    }

    const char *home = getenv(SC_PATH_HOME_ENV);
    if (!home || !path_is_absolute(home)) { return NULL; }
    return join_paths(home, spec->home_fallback);
}

/** Joins two path fragments with a `/`. Returns a heap string or `NULL`. */
static char *join_paths(const char *left, const char *right) {
    size_t total = strlen(left) + 1 + strlen(right) + 1;
    char *joined = malloc(total);
    if (!joined) { return NULL; }

    int written = snprintf(joined, total, "%s/%s", left, right);
    if (written < 0 || (size_t)written >= total) {
        free(joined);
        return NULL;
    }
    return joined;
}

/**
 * Creates every missing component of the absolute path `path` with mode
 * 0700 (`mkdir -p`). Existing components are accepted regardless of their
 * mode. Returns `true` when the full path exists afterwards.
 */
static bool make_dirs(const char *path) {
    char *working_copy = strdup(path);
    if (!working_copy) { return false; }

    char *start = working_copy + 1;
#ifdef _WIN32
    /* Skip a leading drive prefix ("C:") so we never mkdir("C:"). */
    bool drive_letter = (working_copy[0] >= 'A' && working_copy[0] <= 'Z')
                     || (working_copy[0] >= 'a' && working_copy[0] <= 'z');
    if (drive_letter && working_copy[1] == ':') {
        start = working_copy + 2;
        if (SC_IS_SEP(*start)) { start++; }
    }
#endif

    // Create each parent component, then the full path itself. Existing
    // components return EEXIST and are accepted.
    for (char *cursor = start; *cursor; cursor++) {
        if (!SC_IS_SEP(*cursor)) { continue; }
        char separator = *cursor;
        *cursor = '\0';
        if (mkdir(working_copy, PATH_DIR_MODE) != 0 && errno != EEXIST) {
            free(working_copy);
            return false;
        }
        *cursor = separator;
    }
    bool created = mkdir(working_copy, PATH_DIR_MODE) == 0 || errno == EEXIST;

    free(working_copy);
    return created;
}
