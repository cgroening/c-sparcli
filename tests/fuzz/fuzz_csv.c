#include "cli_csv.h"
#include "fuzz_common.h"

#include <string.h>

/**
 * Fuzz target for the CLI's RFC-4180 CSV/TSV parser (`sc_cli_csv_parse`):
 * random byte sequences (quotes, embedded delimiters/newlines, `""` escapes,
 * unterminated quotes) must never crash, leak, read out of bounds, or
 * trigger UB - in both the comma and the tab delimiter mode. Every accessor
 * is exercised across the full row/field range to surface indexing bugs.
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

    static const char delimiters[] = { ',', '\t' };
    for (size_t d = 0; d < sizeof(delimiters); d++) {
        ScCliCsv *csv = sc_cli_csv_parse(input, delimiters[d]);
        if (!csv) {
            continue;   // allocation failure or unterminated quote: valid
        }

        // Walk every row and field, including one past the ragged edge
        // (the accessor contract returns "" there, never NULL).
        size_t rows = sc_cli_csv_rows(csv);
        size_t max_cols = sc_cli_csv_max_cols(csv);
        for (size_t row = 0; row < rows; row++) {
            size_t cols = sc_cli_csv_cols(csv, row);
            if (cols > max_cols) { abort(); }
            for (size_t col = 0; col <= max_cols; col++) {
                const char *field = sc_cli_csv_field(csv, row, col);
                if (!field) { abort(); }
            }
        }
        sc_cli_csv_free(csv);
    }

    free(input);
    return 0;
}
