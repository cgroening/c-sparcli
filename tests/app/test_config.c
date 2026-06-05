#include "test_app.h"
#include "app/sparcli_config.h"
#include "serde/sparcli_serde.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/** Writes `content` to a temp file with the given extension; heap path. */
static char *write_temp(const char *ext, const char *content) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) { dir = "/tmp"; }
    char *path = malloc(strlen(dir) + 64);
    if (!path) { return NULL; }
    sprintf(path, "%s/sparcli-cfg-%d%s", dir, (int)getpid(), ext);
    FILE *f = fopen(path, "w");
    if (!f) { free(path); return NULL; }
    fputs(content, f);
    fclose(f);
    return path;
}


/**
 * Layered config: defaults, file (by extension), env (prefix + `__`
 * nesting + coercion), explicit overrides, precedence ordering and the
 * dotted-path typed getters.
 */
void test_config(void) {
    /* ── defaults + typed getters + fallback ── */
    {
        ScConfig *cfg = sc_config_new();
        ScValue *def = sc_value_object();
        ScValue *server = sc_value_object();
        sc_value_set(server, "port", sc_value_int(8080));
        sc_value_set(server, "host", sc_value_string("0.0.0.0"));
        sc_value_set(def, "server", server);
        sc_value_set(def, "debug", sc_value_bool(false));
        CHECK(sc_config_set_defaults(cfg, def), "config: defaults merged");
        sc_value_free(def);

        CHECK(sc_config_get_int(cfg, "server.port", 0) == 8080,
              "config: dotted int getter");
        CHECK(strcmp(sc_config_get_string(cfg, "server.host", ""),
                     "0.0.0.0") == 0, "config: dotted string getter");
        CHECK(sc_config_get_bool(cfg, "debug", true) == false,
              "config: dotted bool getter");
        CHECK(sc_config_get_int(cfg, "server.missing", 42) == 42,
              "config: fallback for missing path");
        sc_config_free(cfg);
    }

    /* ── set by dotted path (creates intermediate objects) ── */
    {
        ScConfig *cfg = sc_config_new();
        CHECK(sc_config_set(cfg, "a.b.c", sc_value_int(7)),
              "config: set nested path");
        CHECK(sc_config_get_int(cfg, "a.b.c", 0) == 7,
              "config: nested path readable");
        sc_config_free(cfg);
    }

    /* ── env layer: prefix, __ nesting, coercion ── */
    {
        setenv("SPTEST_SERVER__PORT", "9090", 1);
        setenv("SPTEST_DEBUG", "true", 1);
        setenv("SPTEST_RATE", "1.5", 1);
        setenv("SPTEST_NAME", "hello", 1);
        setenv("OTHER_IGNORED", "x", 1);

        ScConfig *cfg = sc_config_new();
        sc_config_load_env(cfg, "SPTEST_");
        CHECK(sc_config_get_int(cfg, "server.port", 0) == 9090,
              "config: env __ nesting + int coercion");
        CHECK(sc_config_get_bool(cfg, "debug", false) == true,
              "config: env bool coercion");
        CHECK(sc_config_get_float(cfg, "rate", 0) == 1.5,
              "config: env float coercion");
        CHECK(strcmp(sc_config_get_string(cfg, "name", ""), "hello") == 0,
              "config: env string kept");
        CHECK(sc_config_get(cfg, "ignored") == NULL,
              "config: env without prefix ignored");
        sc_config_free(cfg);

        unsetenv("SPTEST_SERVER__PORT");
        unsetenv("SPTEST_DEBUG");
        unsetenv("SPTEST_RATE");
        unsetenv("SPTEST_NAME");
        unsetenv("OTHER_IGNORED");
    }

    /* ── file layer + missing file ── */
    {
        ScConfig *cfg = sc_config_new();
        ScParseError err = { 0 };
        CHECK(sc_config_load_file(cfg, "/no/such/file.json", &err)
                  == SC_CONFIG_MISSING,
              "config: missing file is not an error");

        char *path = write_temp(".json",
            "{\"server\":{\"port\":1234},\"debug\":true}");
        CHECK(path && sc_config_load_file(cfg, path, &err) == SC_CONFIG_OK,
              "config: JSON file loads");
        CHECK(sc_config_get_int(cfg, "server.port", 0) == 1234,
              "config: file value read back");
        if (path) { unlink(path); free(path); }
        sc_config_free(cfg);
    }

    /* ── precedence: defaults < file < env < explicit ── */
    {
        ScConfig *cfg = sc_config_new();

        ScValue *def = sc_value_object();
        sc_value_set(def, "port", sc_value_int(1));
        sc_config_set_defaults(cfg, def);
        sc_value_free(def);
        CHECK(sc_config_get_int(cfg, "port", 0) == 1, "config: default wins");

        char *path = write_temp(".json", "{\"port\":2}");
        sc_config_load_file(cfg, path, NULL);
        CHECK(sc_config_get_int(cfg, "port", 0) == 2,
              "config: file overrides default");
        if (path) { unlink(path); free(path); }

        setenv("PREC_PORT", "3", 1);
        sc_config_load_env(cfg, "PREC_");
        CHECK(sc_config_get_int(cfg, "port", 0) == 3,
              "config: env overrides file");
        unsetenv("PREC_PORT");

        ScValue *flags = sc_value_object();
        sc_value_set(flags, "port", sc_value_int(4));
        sc_config_merge(cfg, flags);
        sc_value_free(flags);
        CHECK(sc_config_get_int(cfg, "port", 0) == 4,
              "config: explicit overlay overrides env");

        sc_config_free(cfg);
    }

    /* ── NULL safety ── */
    sc_config_free(NULL);
    CHECK(sc_config_root(NULL) == NULL, "config: root(NULL) safe");
    CHECK(!sc_config_set_defaults(NULL, NULL), "config: set_defaults(NULL)");
}
