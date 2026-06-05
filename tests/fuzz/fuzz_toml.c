#include "serde/sparcli_serde.h"
#include "fuzz_common.h"

#include <stdlib.h>

/**
 * Fuzz target for the TOML reader/writer: arbitrary bytes must never crash,
 * leak or trigger UB. TOML is an untrusted-input surface (config files), so
 * the parser is exercised over random data and any accepted document is
 * round-tripped through both writer modes.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ScParseError err = { 0 };
    ScValue *root = sc_toml_parse((const char *)data, size, &err);
    if (!root) {
        ScError *report = sc_parse_error_to_error(&err);
        sc_error_free(report);
        return 0;
    }

    free(sc_toml_write(root, (ScTomlWriteOpts){ 0 }));
    free(sc_toml_write(root, (ScTomlWriteOpts){ .sort_keys = true }));

    sc_value_free(root);
    return 0;
}
