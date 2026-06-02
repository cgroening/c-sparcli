#include "sparcli.h"
#include "fuzz_common.h"

#include <string.h>

/** Extra bytes needed to wrap a fuzz input in a `[link=…]` construct. */
#define LINK_WRAP_EXTRA 32

static void fuzz_link_wrapped(const char *input, size_t size);

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

    fuzz_link_wrapped(input, size);

    free(input);
    return 0;
}

/**
 * Exercises the `[link=…]` parser path: feeds the raw input once as the
 * link URL and once as the link body.
 */
static void fuzz_link_wrapped(const char *input, size_t size) {
    size_t buffer_size = size + LINK_WRAP_EXTRA;
    char *wrapped = malloc(buffer_size);
    if (!wrapped) {
        return;
    }

    int written = snprintf(wrapped, buffer_size, "[link=%s]body[/link]", input);
    if (written > 0 && (size_t)written < buffer_size) {
        ScText *as_url = sc_markup_parse(wrapped);
        sc_text_free(as_url);
    }

    written = snprintf(wrapped, buffer_size, "[link=https://x]%s[/]", input);
    if (written > 0 && (size_t)written < buffer_size) {
        ScText *as_body = sc_markup_parse(wrapped);
        sc_text_free(as_body);
    }

    free(wrapped);
}
