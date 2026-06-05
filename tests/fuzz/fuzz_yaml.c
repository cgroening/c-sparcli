#include "serde/sparcli_serde.h"
#include "fuzz_common.h"

#include <stdlib.h>

/**
 * Fuzz target for the YAML-subset reader/writer: arbitrary bytes must never
 * crash, leak or trigger UB. YAML is an untrusted-input surface (config files,
 * front matter), so the indentation-sensitive parser is exercised over random
 * data and any accepted document is round-tripped through both writer modes.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ScParseError err = { 0 };
    ScValue *root = sc_yaml_parse((const char *)data, size, &err);
    if (!root) {
        ScError *report = sc_parse_error_to_error(&err);
        sc_error_free(report);
        return 0;
    }

    free(sc_yaml_write(root, (ScYamlWriteOpts){ 0 }));
    free(sc_yaml_write(root, (ScYamlWriteOpts){ .indent = 4, .sort_keys = true }));

    sc_value_free(root);
    return 0;
}
