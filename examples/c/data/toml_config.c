// toml_config.c - parse a TOML config blob, read typed values, and present
// them as a key/value list.
//
// Run (after `make`):
//   make run-example EX=c/data/toml_config

#include <serde/sparcli_serde.h>
#include <sparcli.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *toml =
        "[server]\n"
        "host = \"localhost\"\n"
        "port = 8080\n"
        "tls = true\n";

    ScParseError err = { 0 };
    ScValue *cfg = sc_toml_parse(toml, strlen(toml), &err);
    if (!cfg) {
        sc_die(sc_parse_error_to_error(&err));
    }

    ScValue *server = sc_value_get(cfg, "server");
    int64_t port = 0;
    bool tls = false;
    sc_value_as_int(sc_value_get(server, "port"), &port);
    sc_value_as_bool(sc_value_get(server, "tls"), &tls);

    char port_text[32];
    snprintf(port_text, sizeof port_text, "%lld", (long long)port);

    ScKV *kv = sc_kv_new((ScKVOpts){
        .key_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                       SC_ANSI_COLOR_NONE },
    });
    sc_kv_add(kv, "host", sc_value_as_string(sc_value_get(server, "host")));
    sc_kv_add(kv, "port", port_text);
    sc_kv_add(kv, "tls", tls ? "yes" : "no");
    sc_kv_print(kv);
    sc_kv_free(kv);

    sc_value_free(cfg);
    return 0;
}
