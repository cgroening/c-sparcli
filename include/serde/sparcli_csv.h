#pragma once

#include "core/sparcli_export.h"
#include "serde/sparcli_parse_error.h"

#include <stdbool.h>
#include <stddef.h>

SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_csv.h
 * @brief RFC-4180-style CSV/TSV reader and writer.
 *
 * Tabular, not tree-shaped, so it has its own model (not `ScValue`). Quoted
 * fields support embedded delimiters, newlines and `""` escapes; rows may be
 * ragged (differing field counts). When `has_header` is set, the first row is
 * treated as a header, enabling lookup by column name.
 *
 * Field strings are owned by the document; reader accessors return `""` (never
 * `NULL`) for out-of-range cells, so callers never branch on a null pointer.
 */


/** Dialect options for reading and writing. Zero-init = comma, double quote. */
typedef struct ScCsvOpts {
    /** Field delimiter; `0` selects the default `','` (use `'\t'` for TSV). */
    char delim;

    /** Quote character; `0` selects the default `'"'`. */
    char quote;

    /** When `true`, row 0 is the header (enables `sc_csv_header`/`sc_csv_get`). */
    bool has_header;
} ScCsvOpts;

/** Opaque CSV document; create with `sc_csv_parse` or `sc_csv_new`. */
typedef struct ScCsv ScCsv;


/* ── Reading ───────────────────────────────────────────────────────────── */

/**
 * Parses CSV/TSV text into rows of fields.
 *
 * @param src   Source bytes (need not be NUL-terminated).
 * @param len   Length of `src` in bytes.
 * @param opts  Dialect options.
 * @param err   Optional; filled on failure (e.g. an unterminated quoted
 *              field). May be `NULL`.
 * @return      Parsed document (free with `sc_csv_free`), or `NULL` on a
 *              parse or allocation error.
 */
SPARCLI_EXPORT ScCsv *sc_csv_parse(
    const char *src, size_t len, ScCsvOpts opts, ScParseError *err
);

/** Returns the total number of parsed rows (header included, if any). */
SPARCLI_EXPORT size_t sc_csv_rows(const ScCsv *csv);

/** Returns the widest field count across all rows. */
SPARCLI_EXPORT size_t sc_csv_cols(const ScCsv *csv);

/** Returns the field count of `row` (`0` when out of range). */
SPARCLI_EXPORT size_t sc_csv_row_cols(const ScCsv *csv, size_t row);

/**
 * Returns the field at `row`/`col`.
 *
 * @return  Borrowed, NUL-terminated field text; `""` for an out-of-range
 *          cell (ragged-row padding), never `NULL`.
 */
SPARCLI_EXPORT const char *sc_csv_at(
    const ScCsv *csv, size_t row, size_t col
);

/** Returns true when the document has a usable header row (`has_header` and
 *  at least one row). */
SPARCLI_EXPORT bool sc_csv_has_header(const ScCsv *csv);

/** Returns the header label of `col` (`""` when absent / no header). */
SPARCLI_EXPORT const char *sc_csv_header(const ScCsv *csv, size_t col);

/** Returns the number of data rows (total rows minus the header row). */
SPARCLI_EXPORT size_t sc_csv_data_rows(const ScCsv *csv);

/**
 * Returns the value of column `name` in data row `data_row` (0-based among
 * the data rows, i.e. excluding the header).
 *
 * @return  Borrowed field text, or `""` when there is no header, the name is
 *          unknown, or the cell is out of range.
 */
SPARCLI_EXPORT const char *sc_csv_get(
    const ScCsv *csv, size_t data_row, const char *name
);


/* ── Writing ───────────────────────────────────────────────────────────── */

/**
 * Allocates an empty CSV document for building output.
 *
 * @param opts  Dialect used by `sc_csv_write`.
 * @return      Heap document (free with `sc_csv_free`), or `NULL` on failure.
 */
SPARCLI_EXPORT ScCsv *sc_csv_new(ScCsvOpts opts);

/**
 * Appends one row; field strings are copied.
 *
 * @param csv     Document to append to.
 * @param fields  Array of `count` field strings (`NULL` entries become `""`).
 * @param count   Number of fields in the row.
 * @return        `true` on success, `false` on allocation failure.
 */
SPARCLI_EXPORT bool sc_csv_add_row(
    ScCsv *csv, const char *const *fields, size_t count
);

/**
 * Serializes the document to CSV text (fields quoted only when needed; rows
 * terminated by `\n`).
 *
 * @param csv  Document to serialize.
 * @return     Heap-allocated, NUL-terminated CSV (caller `free`s it), or
 *             `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_csv_write(const ScCsv *csv);

/** Frees a CSV document and its owned fields; safe to pass `NULL`. */
SPARCLI_EXPORT void sc_csv_free(ScCsv *csv);

SPARCLI_END_DECLS
