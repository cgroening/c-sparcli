#include "serde/sparcli_csv.h"
#include "fuzz_common.h"

#include <stdlib.h>

/**
 * Fuzz target for the serde CSV/TSV reader+writer (`sc_csv_parse`): random
 * byte sequences (quotes, embedded delimiters/newlines, `""` escapes,
 * unterminated quotes) must never crash, leak, read out of bounds or trigger
 * UB - in both the comma and the tab dialect. Every accessor is walked across
 * the full row/field range (incl. one past the ragged edge), and any accepted
 * document is round-tripped through the writer.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const char delimiters[] = { ',', '\t' };
    for (size_t d = 0; d < sizeof delimiters; d++) {
        ScParseError err = { 0 };
        ScCsv *csv = sc_csv_parse(
            (const char *)data, size,
            (ScCsvOpts){ .delim = delimiters[d], .has_header = true }, &err
        );
        if (!csv) {
            continue; // allocation failure or unterminated quote: valid
        }

        // Walk every row and field, including one past the ragged edge (the
        // accessor contract returns "" there, never NULL).
        size_t rows = sc_csv_rows(csv);
        size_t cols = sc_csv_cols(csv);
        for (size_t row = 0; row < rows; row++) {
            if (sc_csv_row_cols(csv, row) > cols) { abort(); }
            for (size_t col = 0; col <= cols; col++) {
                if (!sc_csv_at(csv, row, col)) { abort(); }
            }
        }
        // Header lookup path.
        for (size_t col = 0; col < cols; col++) {
            sc_csv_get(csv, 0, sc_csv_header(csv, col));
        }

        // Round-trip through the writer.
        free(sc_csv_write(csv));
        sc_csv_free(csv);
    }
    return 0;
}
