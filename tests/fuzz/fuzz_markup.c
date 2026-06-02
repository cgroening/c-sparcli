#include "sparcli.h"
#include "fuzz_common.h"

#include <string.h>

/**
 * Fuzz target for the inline-markup parser (`sc_markup_parse`): random byte
 * sequences must never crash, leak, or trigger UB - in both the verbatim and
 * the strip-unknown mode.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // The parser takes a NUL-terminated string; embedded NUL bytes simply
    // truncate the input, which is fine for fuzzing purposes.
    char *input = malloc(size + 1);
    if (!input) {
        return 0;
    }
    memcpy(input, data, size);
    input[size] = '\0';

    ScText *verbatim = sc_markup_parse(input);
    sc_text_free(verbatim);

    ScText *stripped = sc_markup_parse_opts(
        input, (ScMarkupOpts){ .strip_unknown = 1 }
    );
    sc_text_free(stripped);

    free(input);
    return 0;
}
