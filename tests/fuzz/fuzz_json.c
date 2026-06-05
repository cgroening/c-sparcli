#include "serde/sparcli_serde.h"
#include "fuzz_common.h"

#include <stdlib.h>

/**
 * Fuzz target for the JSON reader/writer: arbitrary bytes must never crash,
 * leak or trigger UB. JSON is an untrusted-input surface, so the parser is
 * exercised over random data, and any value it accepts is round-tripped
 * through every writer mode to fuzz the serializer too.
 */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ScParseError err = { 0 };
    ScValue *root = sc_json_parse((const char *)data, size, &err);
    if (!root) {
        // The error bridge runs on the failure path too.
        ScError *report = sc_parse_error_to_error(&err);
        sc_error_free(report);
        return 0;
    }

    free(sc_json_write(root, (ScJsonWriteOpts){ 0 }));
    free(sc_json_write(root, (ScJsonWriteOpts){ .indent = 2 }));
    free(sc_json_write(root,
        (ScJsonWriteOpts){ .sort_keys = true, .trailing_newline = true }));

    sc_value_free(root);
    return 0;
}
