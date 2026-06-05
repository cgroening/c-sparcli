#include "serde_internal.h"
#include "serde/sparcli_csv.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file csv.c
 * @brief RFC-4180-style CSV/TSV reader and writer. The reader decodes each
 *        field (unescaping `""` and stripping CRLF) into an owned string via
 *        the shared `ScSerdeBuf`; the writer quotes fields only when needed.
 */


/** Starting slot count for the first field/row insertion. */
#define CSV_MIN_CAPACITY 8

/** Default dialect characters. */
enum {
    CSV_DEFAULT_DELIM = ',',
    CSV_DEFAULT_QUOTE = '"',
};


/** One parsed/built row: an owned, growable array of field strings. */
typedef struct ScCsvRow {
    char  **fields;
    size_t  count;
    size_t  capacity;
} ScCsvRow;

struct ScCsv {
    ScCsvRow *rows;
    size_t    row_count;
    size_t    row_capacity;
    size_t    max_cols;
    bool      has_header;
    char      delim;
    char      quote;
};


// Forward declarations indented to reflect the call hierarchy.
static bool parse_rows(ScCsv *csv, const char *text);
    static bool parse_one_row(ScCsv *csv, const char **cursor);
        static bool decode_quoted(const char **cursor, char quote,
                                  ScSerdeBuf *buf);
        static void decode_plain(const char **cursor, char delim,
                                 ScSerdeBuf *buf);
    static bool row_push_field(ScCsvRow *row, char *field);
    static bool append_row(ScCsv *csv, ScCsvRow row);
    static void row_free(ScCsvRow *row);
static char *dup_string(const char *text);
static bool field_needs_quoting(const char *text, char delim, char quote);


/* ── Reading ───────────────────────────────────────────────────────────── */

ScCsv *sc_csv_parse(
    const char *src, size_t len, ScCsvOpts opts, ScParseError *err
) {
    if (err) {
        *err = (ScParseError){ 0 };
    }
    if (!src) {
        len = 0;
    }

    char *copy = malloc(len + 1);
    if (!copy) {
        if (err) {
            snprintf(err->message, sizeof err->message, "out of memory");
        }
        return NULL;
    }
    if (len > 0) {
        memcpy(copy, src, len);
    }
    copy[len] = '\0';

    ScCsv *csv = calloc(1, sizeof *csv);
    if (!csv) {
        free(copy);
        if (err) {
            snprintf(err->message, sizeof err->message, "out of memory");
        }
        return NULL;
    }
    csv->delim = opts.delim ? opts.delim : CSV_DEFAULT_DELIM;
    csv->quote = opts.quote ? opts.quote : CSV_DEFAULT_QUOTE;
    csv->has_header = opts.has_header;

    bool ok = parse_rows(csv, copy);
    free(copy);
    if (!ok) {
        if (err) {
            err->line = (int)(csv->row_count + 1);
            snprintf(err->message, sizeof err->message,
                     "unterminated quoted field or out of memory");
        }
        sc_csv_free(csv);
        return NULL;
    }
    return csv;
}

/** Parses every row in the NUL-terminated working copy. */
static bool parse_rows(ScCsv *csv, const char *text) {
    const char *cursor = text;
    while (*cursor != '\0') {
        if (!parse_one_row(csv, &cursor)) {
            return false;
        }
    }
    return true;
}

/** Parses one row of fields up to a newline or end of input. */
static bool parse_one_row(ScCsv *csv, const char **cursor) {
    ScCsvRow row = { 0 };
    const char *p = *cursor;

    for (;;) {
        ScSerdeBuf buf = { 0 };
        if (*p == csv->quote) {
            if (!decode_quoted(&p, csv->quote, &buf)) {
                sc_serde_buf_free(&buf);
                row_free(&row);
                return false;
            }
        } else {
            decode_plain(&p, csv->delim, &buf);
        }

        // Remember and consume the byte that ended the field.
        char terminator = *p;
        if (terminator != '\0') {
            p++;
        }
        // Strip the CR of a CRLF line ending.
        if (terminator == '\n' && buf.len > 0
            && buf.data[buf.len - 1] == '\r') {
            buf.len--;
            buf.data[buf.len] = '\0';
        }

        char *field = sc_serde_buf_finish(&buf);
        if (!field || !row_push_field(&row, field)) {
            free(field);
            row_free(&row);
            return false;
        }

        if (terminator != csv->delim) {
            break; // newline or end of input: the row is complete
        }
    }

    if (row.count > csv->max_cols) {
        csv->max_cols = row.count;
    }
    if (!append_row(csv, row)) {
        row_free(&row);
        return false;
    }
    *cursor = p;
    return true;
}

/** Decodes a quoted field; `""` is a literal quote. Fails when unterminated. */
static bool decode_quoted(
    const char **cursor, char quote, ScSerdeBuf *buf
) {
    const char *p = *cursor + 1; // skip opening quote
    while (*p != '\0') {
        if (p[0] == quote && p[1] == quote) {
            sc_serde_buf_append_char(buf, quote);
            p += 2;
            continue;
        }
        if (p[0] == quote) {
            *cursor = p + 1; // skip closing quote
            return true;
        }
        sc_serde_buf_append_char(buf, *p);
        p++;
    }
    *cursor = p;
    return false;
}

/** Decodes an unquoted field up to the delimiter, newline or end. */
static void decode_plain(
    const char **cursor, char delim, ScSerdeBuf *buf
) {
    const char *p = *cursor;
    while (*p != '\0' && *p != delim && *p != '\n') {
        sc_serde_buf_append_char(buf, *p);
        p++;
    }
    *cursor = p;
}


/* ── Reader accessors ──────────────────────────────────────────────────── */

size_t sc_csv_rows(const ScCsv *csv) {
    return csv ? csv->row_count : 0;
}

size_t sc_csv_cols(const ScCsv *csv) {
    return csv ? csv->max_cols : 0;
}

size_t sc_csv_row_cols(const ScCsv *csv, size_t row) {
    if (!csv || row >= csv->row_count) {
        return 0;
    }
    return csv->rows[row].count;
}

const char *sc_csv_at(const ScCsv *csv, size_t row, size_t col) {
    if (!csv || row >= csv->row_count || col >= csv->rows[row].count) {
        return "";
    }
    return csv->rows[row].fields[col];
}

bool sc_csv_has_header(const ScCsv *csv) {
    return csv && csv->has_header && csv->row_count > 0;
}

const char *sc_csv_header(const ScCsv *csv, size_t col) {
    if (!sc_csv_has_header(csv)) {
        return "";
    }
    return sc_csv_at(csv, 0, col);
}

size_t sc_csv_data_rows(const ScCsv *csv) {
    if (!csv) {
        return 0;
    }
    return csv->row_count - (sc_csv_has_header(csv) ? 1 : 0);
}

const char *sc_csv_get(const ScCsv *csv, size_t data_row, const char *name) {
    if (!sc_csv_has_header(csv) || !name) {
        return "";
    }
    const ScCsvRow *header = &csv->rows[0];
    for (size_t col = 0; col < header->count; col++) {
        if (strcmp(header->fields[col], name) == 0) {
            return sc_csv_at(csv, data_row + 1, col);
        }
    }
    return "";
}


/* ── Writing ───────────────────────────────────────────────────────────── */

ScCsv *sc_csv_new(ScCsvOpts opts) {
    ScCsv *csv = calloc(1, sizeof *csv);
    if (!csv) {
        return NULL;
    }
    csv->delim = opts.delim ? opts.delim : CSV_DEFAULT_DELIM;
    csv->quote = opts.quote ? opts.quote : CSV_DEFAULT_QUOTE;
    csv->has_header = opts.has_header;
    return csv;
}

bool sc_csv_add_row(ScCsv *csv, const char *const *fields, size_t count) {
    if (!csv || (count > 0 && !fields)) {
        return false;
    }

    ScCsvRow row = { 0 };
    for (size_t i = 0; i < count; i++) {
        char *owned = dup_string(fields[i] ? fields[i] : "");
        if (!owned || !row_push_field(&row, owned)) {
            free(owned);
            row_free(&row);
            return false;
        }
    }

    if (count > csv->max_cols) {
        csv->max_cols = count;
    }
    if (!append_row(csv, row)) {
        row_free(&row);
        return false;
    }
    return true;
}

char *sc_csv_write(const ScCsv *csv) {
    ScSerdeBuf buf = { 0 };
    if (csv) {
        for (size_t r = 0; r < csv->row_count; r++) {
            const ScCsvRow *row = &csv->rows[r];
            for (size_t c = 0; c < row->count; c++) {
                if (c > 0) {
                    sc_serde_buf_append_char(&buf, csv->delim);
                }
                const char *field = row->fields[c];
                if (!field_needs_quoting(field, csv->delim, csv->quote)) {
                    sc_serde_buf_append_str(&buf, field);
                    continue;
                }
                sc_serde_buf_append_char(&buf, csv->quote);
                for (const char *s = field; *s; s++) {
                    if (*s == csv->quote) {
                        sc_serde_buf_append_char(&buf, csv->quote);
                    }
                    sc_serde_buf_append_char(&buf, *s);
                }
                sc_serde_buf_append_char(&buf, csv->quote);
            }
            sc_serde_buf_append_char(&buf, '\n');
        }
    }
    return sc_serde_buf_finish(&buf);
}

void sc_csv_free(ScCsv *csv) {
    if (!csv) {
        return;
    }
    for (size_t i = 0; i < csv->row_count; i++) {
        row_free(&csv->rows[i]);
    }
    free(csv->rows);
    free(csv);
}


/* ── Internal helpers ──────────────────────────────────────────────────── */

/** Appends an owned field string to a row, growing as needed. */
static bool row_push_field(ScCsvRow *row, char *field) {
    if (row->count == row->capacity) {
        if (row->capacity > SIZE_MAX / 2) {
            return false;
        }
        size_t new_cap = row->capacity ? row->capacity * 2 : CSV_MIN_CAPACITY;
        if (new_cap > SIZE_MAX / sizeof *row->fields) {
            return false;
        }
        char **grown = realloc(row->fields, new_cap * sizeof *row->fields);
        if (!grown) {
            return false;
        }
        row->fields = grown;
        row->capacity = new_cap;
    }
    row->fields[row->count] = field;
    row->count++;
    return true;
}

/** Appends a row (by value) to the document, growing as needed. */
static bool append_row(ScCsv *csv, ScCsvRow row) {
    if (csv->row_count == csv->row_capacity) {
        if (csv->row_capacity > SIZE_MAX / 2) {
            return false;
        }
        size_t new_cap = csv->row_capacity ? csv->row_capacity * 2
                                           : CSV_MIN_CAPACITY;
        if (new_cap > SIZE_MAX / sizeof *csv->rows) {
            return false;
        }
        ScCsvRow *grown = realloc(csv->rows, new_cap * sizeof *csv->rows);
        if (!grown) {
            return false;
        }
        csv->rows = grown;
        csv->row_capacity = new_cap;
    }
    csv->rows[csv->row_count] = row;
    csv->row_count++;
    return true;
}

/** Frees a row's owned field strings. */
static void row_free(ScCsvRow *row) {
    for (size_t i = 0; i < row->count; i++) {
        free(row->fields[i]);
    }
    free(row->fields);
    row->fields = NULL;
    row->count = 0;
    row->capacity = 0;
}

/** Copies a string; treats `NULL` as `""`. */
static char *dup_string(const char *text) {
    if (!text) {
        text = "";
    }
    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy) {
        memcpy(copy, text, size);
    }
    return copy;
}

/** Reports whether a field must be quoted on output. */
static bool field_needs_quoting(const char *text, char delim, char quote) {
    for (const char *s = text; *s; s++) {
        if (*s == delim || *s == quote || *s == '\n' || *s == '\r') {
            return true;
        }
    }
    return false;
}
