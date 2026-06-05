// json_roundtrip.c - parse JSON into the ScValue tree, navigate it, and
// pretty-print it back out.
//
// Run (after `make`):
//   make run-example EX=c/data/json_roundtrip

#include <serde/sparcli_serde.h>
#include <sparcli.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *json =
        "{\"name\":\"sparcli\",\"version\":[0,1,0],\"stable\":false}";

    // Parse: NULL on error, with the location filled into `err`.
    ScParseError err = { 0 };
    ScValue *root = sc_json_parse(json, strlen(json), &err);
    if (!root) {
        sc_die(sc_parse_error_to_error(&err)); // renders + exits
    }

    // Navigate the tree (accessors return false / NULL for type mismatches).
    const char *name = sc_value_as_string(sc_value_get(root, "name"));
    int64_t major = 0;
    sc_value_as_int(sc_value_at(sc_value_get(root, "version"), 0), &major);

    sc_markup_println("[bold]Parsed JSON[/]");
    printf("name  = %s\n", name ? name : "(none)");
    printf("major = %lld\n", (long long)major);

    // Re-serialize with indentation (compact when indent is 0).
    char *pretty = sc_json_write(root, (ScJsonWriteOpts){ .indent = 2 });
    sc_rule_str(
        "pretty",
        (ScRuleOpts){ .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE }
    );
    printf("%s\n", pretty ? pretty : "");
    free(pretty);

    sc_value_free(root);
    return 0;
}
