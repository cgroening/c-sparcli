/**
 * @file cli_csv.h
 * Minimal RFC-4180-style CSV/TSV parser for the `table` and `fuzzy`
 * subcommands. Supports quoted fields (embedded delimiters, newlines and
 * `""` escapes); rows may have differing field counts (ragged tables).
 */
#pragma once

#include <stddef.h>

/** Parsed CSV document (opaque). Create with `sc_cli_csv_parse`. */
typedef struct ScCliCsv ScCliCsv;

/**
 * Parses CSV/TSV text into rows and fields.
 *
 * @param data   Source text; the parser keeps its own copy.
 * @param delim  Field delimiter, e.g. `','` or `'\t'`.
 * @return       Parsed document (free with `sc_cli_csv_free`), or `NULL`
 *               on allocation failure or unterminated quoted field.
 */
ScCliCsv *sc_cli_csv_parse(const char *data, char delim);

/** Returns the number of rows. */
size_t sc_cli_csv_rows(const ScCliCsv *csv);

/** Returns the number of fields in row `row`. */
size_t sc_cli_csv_cols(const ScCliCsv *csv, size_t row);

/** Returns the widest field count across all rows. */
size_t sc_cli_csv_max_cols(const ScCliCsv *csv);

/**
 * Returns the field at `row`/`col` as a NUL-terminated string.
 *
 * @return  The field text (owned by the document), or `""` when `col` is
 *          beyond the row's field count (ragged row padding).
 */
const char *sc_cli_csv_field(const ScCliCsv *csv, size_t row, size_t col);

/** Frees a parsed document. */
void sc_cli_csv_free(ScCliCsv *csv);
