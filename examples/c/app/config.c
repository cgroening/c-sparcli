/*
 * config.c - layered configuration: defaults < env < explicit overrides.
 *
 * Config depends on the serde data model, so it is opt-in: include
 * <app/sparcli_config.h> explicitly (it is not in the sparcli.h umbrella).
 *
 * Run (after `make`):
 *   MYAPP_SERVER__PORT=9090 make run-example EX=c/app/config
 */

#include <sparcli.h>
#include <serde/sparcli_serde.h>
#include <app/sparcli_config.h>

#include <stdio.h>


int main(void) {
    ScConfig *cfg = sc_config_new();

    // Layer 1: built-in defaults.
    ScValue *defaults = sc_value_object();
    ScValue *server = sc_value_object();
    sc_value_set(server, "host", sc_value_string("127.0.0.1"));
    sc_value_set(server, "port", sc_value_int(8080));
    sc_value_set(defaults, "server", server);
    sc_value_set(defaults, "debug", sc_value_bool(false));
    sc_config_set_defaults(cfg, defaults);
    sc_value_free(defaults);

    // Layer 3: environment (MYAPP_SERVER__PORT=… overrides server.port).
    sc_config_load_env(cfg, "MYAPP_");

    // Layer 4: explicit overrides (e.g. parsed CLI flags).
    sc_config_set(cfg, "debug", sc_value_bool(true));

    printf("server.host = %s\n",
           sc_config_get_string(cfg, "server.host", "?"));
    printf("server.port = %lld\n",
           (long long)sc_config_get_int(cfg, "server.port", 0));
    printf("debug       = %s\n",
           sc_config_get_bool(cfg, "debug", false) ? "true" : "false");

    sc_config_free(cfg);
    return 0;
}
