#include "app/sparcli_config.h"
#include "serde/sparcli_json.h"
#include "serde/sparcli_toml.h"
#include "serde/sparcli_yaml.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/** The process environment (POSIX). */
extern char **environ;

/** Max dotted-path segments handled by the path helpers. */
#define CONFIG_MAX_SEGMENTS 32


struct ScConfig {
    ScValue *root;   /* always an object */
};


/* ── path helpers ────────────────────────────────────────────────────────── */

/**
 * Wraps `leaf` in a chain of objects following the dotted `path`, e.g.
 * `"a.b"` + leaf → `{ "a": { "b": leaf } }`. Takes ownership of `leaf`
 * (frees it on failure). Returns the outermost object, or `NULL`.
 */
static ScValue *wrap_path(const char *path, ScValue *leaf) {
    if (!path || !leaf) { sc_value_free(leaf); return NULL; }

    const char *segs[CONFIG_MAX_SEGMENTS];
    size_t lens[CONFIG_MAX_SEGMENTS];
    size_t n = 0;

    const char *p = path;
    while (*p && n < CONFIG_MAX_SEGMENTS) {
        const char *start = p;
        while (*p && *p != '.') { p++; }
        if (p > start) { segs[n] = start; lens[n] = (size_t)(p - start); n++; }
        if (*p == '.') { p++; }
    }
    if (n == 0) { sc_value_free(leaf); return NULL; }

    ScValue *node = leaf;
    for (size_t i = n; i-- > 0;) {
        char key[256];
        size_t klen = lens[i] < sizeof key - 1 ? lens[i] : sizeof key - 1;
        memcpy(key, segs[i], klen);
        key[klen] = '\0';

        ScValue *obj = sc_value_object();
        if (!obj) { sc_value_free(node); return NULL; }
        if (!sc_value_set(obj, key, node)) {
            /* set failed: node ownership stays with us, obj is empty */
            sc_value_free(node);
            sc_value_free(obj);
            return NULL;
        }
        node = obj;
    }
    return node;
}

/** Merges `overlay` into the config root and frees `overlay`. */
static bool merge_and_free(ScConfig *cfg, ScValue *overlay) {
    if (!overlay) { return false; }
    bool ok = sc_value_merge(cfg->root, overlay);
    sc_value_free(overlay);
    return ok;
}


/* ── scalar inference for env values ─────────────────────────────────────── */

/** Coerces an env string to bool/int/float when it looks like one. */
static ScValue *infer_scalar(const char *s) {
    if (strcmp(s, "true") == 0)  { return sc_value_bool(true); }
    if (strcmp(s, "false") == 0) { return sc_value_bool(false); }

    if (s[0] != '\0') {
        char *end = NULL;
        long long iv = strtoll(s, &end, 10);
        if (end && *end == '\0') { return sc_value_int((int64_t)iv); }
        end = NULL;
        double dv = strtod(s, &end);
        if (end && *end == '\0') { return sc_value_float(dv); }
    }
    return sc_value_string(s);
}


/* ── lifecycle ───────────────────────────────────────────────────────────── */

ScConfig *sc_config_new(void) {
    ScConfig *cfg = calloc(1, sizeof *cfg);
    if (!cfg) { return NULL; }
    cfg->root = sc_value_object();
    if (!cfg->root) { free(cfg); return NULL; }
    return cfg;
}

void sc_config_free(ScConfig *cfg) {
    if (!cfg) { return; }
    sc_value_free(cfg->root);
    free(cfg);
}


/* ── layers ──────────────────────────────────────────────────────────────── */

bool sc_config_set_defaults(ScConfig *cfg, const ScValue *defaults) {
    if (!cfg || !defaults) { return false; }
    return sc_value_merge(cfg->root, defaults);
}

bool sc_config_merge(ScConfig *cfg, const ScValue *overlay) {
    if (!cfg || !overlay) { return false; }
    return sc_value_merge(cfg->root, overlay);
}

/** Returns the parser for a path's extension, or NULL when unknown. */
static ScValue *parse_by_ext(const char *path, ScParseError *err) {
    const char *dot = strrchr(path, '.');
    if (!dot) { return NULL; }
    if (strcmp(dot, ".json") == 0) { return sc_json_parse_file(path, err); }
    if (strcmp(dot, ".toml") == 0) { return sc_toml_parse_file(path, err); }
    if (strcmp(dot, ".yaml") == 0 || strcmp(dot, ".yml") == 0) {
        return sc_yaml_parse_file(path, err);
    }
    return NULL;
}

ScConfigStatus sc_config_load_file(ScConfig *cfg, const char *path,
                                   ScParseError *err) {
    if (!cfg || !path) { return SC_CONFIG_ERROR; }
    if (access(path, F_OK) != 0) { return SC_CONFIG_MISSING; }

    const char *dot = strrchr(path, '.');
    bool known = dot && (strcmp(dot, ".json") == 0 || strcmp(dot, ".toml") == 0
                         || strcmp(dot, ".yaml") == 0
                         || strcmp(dot, ".yml") == 0);
    if (!known) { return SC_CONFIG_ERROR; }   /* unknown format */

    ScValue *parsed = parse_by_ext(path, err);
    if (!parsed) { return SC_CONFIG_ERROR; }
    if (sc_value_type(parsed) != SC_VALUE_OBJECT) {
        sc_value_free(parsed);
        return SC_CONFIG_ERROR;
    }

    bool ok = sc_value_merge(cfg->root, parsed);
    sc_value_free(parsed);
    return ok ? SC_CONFIG_OK : SC_CONFIG_ERROR;
}

/**
 * Builds the dotted key path for one env var name (after the prefix):
 * lowercased, `__` → `.`, single `_` kept. Writes into `out` (size `cap`).
 */
static void env_key_path(const char *name, char *out, size_t cap) {
    size_t w = 0;
    for (const char *p = name; *p && w + 1 < cap;) {
        if (p[0] == '_' && p[1] == '_') { out[w++] = '.'; p += 2; }
        else { out[w++] = (char)tolower((unsigned char)*p); p++; }
    }
    out[w] = '\0';
}

void sc_config_load_env(ScConfig *cfg, const char *prefix) {
    if (!cfg || !prefix || !prefix[0] || !environ) { return; }
    size_t plen = strlen(prefix);

    for (char **e = environ; *e; e++) {
        if (strncmp(*e, prefix, plen) != 0) { continue; }
        const char *eq = strchr(*e, '=');
        if (!eq || eq == *e + plen) { continue; }   /* no key after prefix */

        size_t name_len = (size_t)(eq - (*e + plen));
        char *name = malloc(name_len + 1);
        if (!name) { continue; }
        memcpy(name, *e + plen, name_len);
        name[name_len] = '\0';

        char path[512];
        env_key_path(name, path, sizeof path);
        free(name);

        ScValue *leaf = infer_scalar(eq + 1);
        ScValue *overlay = wrap_path(path, leaf);
        merge_and_free(cfg, overlay);
    }
}

bool sc_config_set(ScConfig *cfg, const char *path, ScValue *value) {
    if (!cfg || !path) { sc_value_free(value); return false; }
    ScValue *overlay = wrap_path(path, value);
    return merge_and_free(cfg, overlay);
}


/* ── access ──────────────────────────────────────────────────────────────── */

const ScValue *sc_config_root(const ScConfig *cfg) {
    return cfg ? cfg->root : NULL;
}

const ScValue *sc_config_get(const ScConfig *cfg, const char *path) {
    if (!cfg) { return NULL; }
    return sc_value_path(cfg->root, path ? path : "");
}

bool sc_config_get_bool(const ScConfig *cfg, const char *path, bool fallback) {
    bool out;
    const ScValue *v = sc_config_get(cfg, path);
    return sc_value_as_bool(v, &out) ? out : fallback;
}

int64_t sc_config_get_int(const ScConfig *cfg, const char *path,
                          int64_t fallback) {
    int64_t out;
    const ScValue *v = sc_config_get(cfg, path);
    return sc_value_as_int(v, &out) ? out : fallback;
}

double sc_config_get_float(const ScConfig *cfg, const char *path,
                           double fallback) {
    double out;
    const ScValue *v = sc_config_get(cfg, path);
    return sc_value_as_float(v, &out) ? out : fallback;
}

const char *sc_config_get_string(const ScConfig *cfg, const char *path,
                                 const char *fallback) {
    const ScValue *v = sc_config_get(cfg, path);
    const char *s = sc_value_as_string(v);
    return s ? s : fallback;
}
