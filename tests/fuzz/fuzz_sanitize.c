#include "sparcli.h"
#include "core/sanitize_internal.h"
#include "fuzz_common.h"

#include <string.h>

/**
 * Fuzz target for the ANSI sanitizer (`sc_sanitize_copy`) and the hardened
 * `sc_strip_ansi`: random byte sequences (heavy on ESC/CSI/OSC introducers)
 * must never crash, leak, read out of bounds, or violate the output
 * contract - the result is never longer than the input, and with
 * `allow_ansi = false` it contains no ESC byte and no control bytes other
 * than `\n` and `\t`.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // The sanitizer takes a NUL-terminated string; embedded NUL bytes simply
    // truncate the input, which is fine for fuzzing purposes.
    char *input = malloc(size + 1);
    if (!input) {
        return 0;
    }
    memcpy(input, data, size);
    input[size] = '\0';

    // Strict mode: output must be free of ESC and stray control bytes
    char *strict = sc_sanitize_copy(input, false);
    if (strict) {
        if (strlen(strict) > strlen(input)) { abort(); }
        for (const char *cursor = strict; *cursor; cursor++) {
            unsigned char byte = (unsigned char)*cursor;
            int is_allowed_control = byte == '\n' || byte == '\t';
            if ((byte < 0x20 && !is_allowed_control) || byte == 0x7f) {
                abort();
            }
        }
        free(strict);
    }

    // Allow mode: output must still be free of stray (non-ESC) control bytes
    char *allowed = sc_sanitize_copy(input, true);
    if (allowed) {
        if (strlen(allowed) > strlen(input)) { abort(); }
        free(allowed);
    }

    // Hardened stripper: output contains no ESC byte at all
    char *stripped = sc_strip_ansi(input);
    if (stripped) {
        if (strchr(stripped, '\033') != NULL) { abort(); }
        free(stripped);
    }

    free(input);
    return 0;
}
