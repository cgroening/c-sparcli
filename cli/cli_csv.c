/**
 * @file cli_csv.c
 * In-place CSV/TSV parser: quotes are unescaped and every field is
 * NUL-terminated inside one owned copy of the input, so fields need no
 * further allocations.
 */
#include "cli_csv.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/** Index range of one row inside the flat field array. */
typedef struct ScCliCsvRow {
    size_t start; /**< Index of the row's first field. */
    size_t count; /**< Number of fields in the row. */
} ScCliCsvRow;

struct ScCliCsv {
    char         *data;       /**< Owned copy; fields point into it. */
    char        **fields;     /**< Flat array of field pointers. */
    size_t        field_count;
    size_t        field_cap;
    ScCliCsvRow  *rows;
    size_t        row_count;
    size_t        row_cap;
    size_t        max_cols;
};

/** Read/write cursors for the in-place parse. */
typedef struct ScCliCsvCursor {
    char *read;  /**< Next input byte. */
    char *write; /**< Next output byte (always <= read). */
} ScCliCsvCursor;

static bool parse_document(ScCliCsv *csv, char delim);
    static bool parse_row(ScCliCsv *csv, ScCliCsvCursor *cur, char delim);
        static bool parse_quoted_field(ScCliCsvCursor *cur);
        static void parse_plain_field(ScCliCsvCursor *cur, char delim);
    static bool push_field(ScCliCsv *csv, char *field);
    static bool push_row(ScCliCsv *csv, ScCliCsvRow row);

ScCliCsv *sc_cli_csv_parse(const char *data, char delim) {
    ScCliCsv *csv = calloc(1, sizeof(*csv));
    if (csv == NULL) {
        return NULL;
    }

    csv->data = strdup(data);
    if (csv->data == NULL || !parse_document(csv, delim)) {
        sc_cli_csv_free(csv);
        return NULL;
    }
    return csv;
}

size_t sc_cli_csv_rows(const ScCliCsv *csv) {
    return csv->row_count;
}

size_t sc_cli_csv_cols(const ScCliCsv *csv, size_t row) {
    if (row >= csv->row_count) {
        return 0;
    }
    return csv->rows[row].count;
}

size_t sc_cli_csv_max_cols(const ScCliCsv *csv) {
    return csv->max_cols;
}

const char *sc_cli_csv_field(const ScCliCsv *csv, size_t row, size_t col) {
    if (row >= csv->row_count || col >= csv->rows[row].count) {
        return "";
    }
    return csv->fields[csv->rows[row].start + col];
}

void sc_cli_csv_free(ScCliCsv *csv) {
    if (csv == NULL) {
        return;
    }
    free(csv->data);
    free(csv->fields);
    free(csv->rows);
    free(csv);
}

static bool parse_document(ScCliCsv *csv, char delim) {
    ScCliCsvCursor cur = { .read = csv->data, .write = csv->data };

    while (*cur.read != '\0') {
        if (!parse_row(csv, &cur, delim)) {
            return false;
        }
    }
    return true;
}

static bool parse_row(ScCliCsv *csv, ScCliCsvCursor *cur, char delim) {
    ScCliCsvRow row = { .start = csv->field_count, .count = 0 };

    while (true) {
        char *field_start = cur->write;

        /* Parse one field (quoted or plain). */
        if (*cur->read == '"') {
            if (!parse_quoted_field(cur)) {
                return false;
            }
        } else {
            parse_plain_field(cur, delim);
        }

        /* Terminate the field after remembering what ended it. The read
           cursor is moved past the terminator first, so the NUL never
           overwrites un-read input (write <= read holds throughout). */
        char terminator = *cur->read;
        if (terminator != '\0') {
            cur->read++;
        }
        if (terminator == '\n' && cur->write > field_start
            && cur->write[-1] == '\r') {
            cur->write--; /* strip CR from CRLF line endings */
        }
        *cur->write = '\0';
        cur->write++;

        if (!push_field(csv, field_start)) {
            return false;
        }
        row.count++;

        if (terminator != delim) {
            break; /* newline or end of input: row is complete */
        }
    }

    if (row.count > csv->max_cols) {
        csv->max_cols = row.count;
    }
    return push_row(csv, row);
}

/* Consumes a quoted field: `""` becomes a literal quote, the delimiter and
   newlines are ordinary content. Fails on a missing closing quote. */
static bool parse_quoted_field(ScCliCsvCursor *cur) {
    cur->read++; /* opening quote */

    while (*cur->read != '\0') {
        if (cur->read[0] == '"' && cur->read[1] == '"') {
            *cur->write = '"';
            cur->write++;
            cur->read += 2;
            continue;
        }
        if (cur->read[0] == '"') {
            cur->read++; /* closing quote */
            return true;
        }
        *cur->write = *cur->read;
        cur->write++;
        cur->read++;
    }
    return false; /* unterminated quote */
}

/* Consumes an unquoted field up to the delimiter, newline or end. */
static void parse_plain_field(ScCliCsvCursor *cur, char delim) {
    while (*cur->read != '\0' && *cur->read != delim
           && *cur->read != '\n') {
        *cur->write = *cur->read;
        cur->write++;
        cur->read++;
    }
}

enum {
    SC_CLI_CSV_FIELD_CAP_INITIAL = 16,
    SC_CLI_CSV_ROW_CAP_INITIAL   = 8,
};

static bool push_field(ScCliCsv *csv, char *field) {
    if (csv->field_count == csv->field_cap) {
        size_t new_cap = (csv->field_cap == 0) ? SC_CLI_CSV_FIELD_CAP_INITIAL
                                               : csv->field_cap * 2;
        char **grown   = realloc(csv->fields, new_cap * sizeof(*grown));
        if (grown == NULL) {
            return false;
        }
        csv->fields    = grown;
        csv->field_cap = new_cap;
    }
    csv->fields[csv->field_count] = field;
    csv->field_count++;
    return true;
}

static bool push_row(ScCliCsv *csv, ScCliCsvRow row) {
    if (csv->row_count == csv->row_cap) {
        size_t       new_cap = (csv->row_cap == 0) ? SC_CLI_CSV_ROW_CAP_INITIAL
                                                   : csv->row_cap * 2;
        ScCliCsvRow *grown   = realloc(csv->rows, new_cap * sizeof(*grown));
        if (grown == NULL) {
            return false;
        }
        csv->rows    = grown;
        csv->row_cap = new_cap;
    }
    csv->rows[csv->row_count] = row;
    csv->row_count++;
    return true;
}
