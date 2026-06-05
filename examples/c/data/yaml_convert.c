// yaml_convert.c - parse YAML and emit JSON. Both formats share the one
// ScValue model, so conversion is just "parse one, write the other".
//
// Run (after `make`):
//   make run-example EX=c/data/yaml_convert

#include <serde/sparcli_serde.h>
#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *yaml =
        "name: sparcli\n"
        "tags:\n"
        "  - cli\n"
        "  - tui\n"
        "nested:\n"
        "  enabled: true\n";

    ScParseError err = { 0 };
    ScValue *root = sc_yaml_parse(yaml, strlen(yaml), &err);
    if (!root) {
        sc_die(sc_parse_error_to_error(&err));
    }

    char *json = sc_json_write(root, (ScJsonWriteOpts){ .indent = 2 });
    sc_markup_println("[bold]YAML → JSON[/] (same ScValue tree)");
    printf("%s\n", json ? json : "");
    free(json);

    sc_value_free(root);
    return 0;
}
