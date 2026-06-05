// config_merge.c - a realistic config workflow with the ScValue ergonomics:
// deep-merge defaults with a user overlay, read via a dotted path and typed
// getters, drop a key, and round-trip the result through a file.
//
// Run (after `make`):
//   make run-example EX=c/data/config_merge

#include <serde/sparcli_serde.h>
#include <sparcli.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    // Built-in defaults and a user overlay (e.g. loaded from a config file).
    const char *defaults =
        "{\"host\":\"localhost\",\"port\":8080,"
        "\"tls\":{\"enabled\":false,\"verify\":true},\"debug\":true}";
    const char *user = "{\"port\":443,\"tls\":{\"enabled\":true}}";

    ScValue *cfg = sc_json_parse(defaults, strlen(defaults), NULL);
    ScValue *overlay = sc_json_parse(user, strlen(user), NULL);
    sc_value_merge(cfg, overlay);   // deep overlay: tls.verify is preserved
    sc_value_free(overlay);

    sc_value_remove(cfg, "debug");  // drop a key we don't want to persist

    // Read with typed getters (with fallbacks) and a dotted path.
    int64_t port = sc_value_get_int_or(cfg, "port", 80);
    const char *host = sc_value_get_string_or(cfg, "host", "?");
    bool tls = false;
    bool verify = false;
    sc_value_as_bool(sc_value_path(cfg, "tls.enabled"), &tls);
    sc_value_as_bool(sc_value_path(cfg, "tls.verify"), &verify);

    ScKV *kv = sc_kv_new((ScKVOpts){
        .key_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                       SC_ANSI_COLOR_NONE },
    });
    char port_text[32];
    snprintf(port_text, sizeof port_text, "%lld", (long long)port);
    sc_kv_add(kv, "host", host);
    sc_kv_add(kv, "port", port_text);
    sc_kv_add(kv, "tls.enabled", tls ? "yes" : "no");
    sc_kv_add(kv, "tls.verify (kept)", verify ? "yes" : "no");
    sc_markup_println("[bold]Effective config[/] (defaults + overlay)");
    sc_kv_print(kv);
    sc_kv_free(kv);

    // Persist it and read it straight back to show the file helpers.
    char path[] = "/tmp/sparcli_config_example_XXXXXX";
    int fd = mkstemp(path);
    if (fd >= 0) {
        close(fd);
        sc_json_write_file(cfg, path, (ScJsonWriteOpts){ .indent = 2 });
        ScValue *reloaded = sc_json_parse_file(path, NULL);
        char *json = sc_json_write(reloaded, (ScJsonWriteOpts){ .indent = 2 });
        sc_markup_println("\n[bold]Reloaded from file[/]");
        printf("%s\n", json ? json : "");
        free(json);
        sc_value_free(reloaded);
        unlink(path);
    }

    sc_value_free(cfg);
    return 0;
}
