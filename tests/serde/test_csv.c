#include "test_serde.h"


// Forward declarations indented to reflect the call hierarchy.
static void check_basic_rows(void);
static void check_quoting_and_escapes(void);
static void check_crlf_and_ragged(void);
static void check_tsv(void);
static void check_header_lookup(void);
static void check_writer_round_trip(void);
static void check_unterminated_quote(void);


/** Exercises the CSV reader and writer end-to-end. */
void test_serde_csv(void) {
    check_basic_rows();
    check_quoting_and_escapes();
    check_crlf_and_ragged();
    check_tsv();
    check_header_lookup();
    check_writer_round_trip();
    check_unterminated_quote();
}


/* ── Reading ─────────────────────────────────────────────────────────────── */

static void check_basic_rows(void) {
    const char *text = "a,b,c\n1,2,3\n";
    ScCsv *csv = sc_csv_parse(text, strlen(text), (ScCsvOpts){ 0 }, NULL);

    CHECK(sc_csv_rows(csv) == 2, "parse: row count");
    CHECK(sc_csv_cols(csv) == 3, "parse: widest column count");
    CHECK(strcmp(sc_csv_at(csv, 1, 2), "3") == 0, "parse: cell by row/col");
    CHECK(sc_csv_at(csv, 9, 0)[0] == '\0',
          "parse: out-of-range cell returns empty string");

    sc_csv_free(csv);
}

static void check_quoting_and_escapes(void) {
    // A quoted field with an embedded comma, newline and a "" escape.
    const char *text = "\"a,b\",\"line1\nline2\",\"he said \"\"hi\"\"\"\n";
    ScCsv *csv = sc_csv_parse(text, strlen(text), (ScCsvOpts){ 0 }, NULL);

    CHECK(sc_csv_rows(csv) == 1 && sc_csv_row_cols(csv, 0) == 3,
          "parse: quoted fields keep delimiters out of the split");
    CHECK(strcmp(sc_csv_at(csv, 0, 0), "a,b") == 0,
          "parse: embedded delimiter preserved");
    CHECK(strcmp(sc_csv_at(csv, 0, 1), "line1\nline2") == 0,
          "parse: embedded newline preserved");
    CHECK(strcmp(sc_csv_at(csv, 0, 2), "he said \"hi\"") == 0,
          "parse: \"\" unescapes to a literal quote");

    sc_csv_free(csv);
}

static void check_crlf_and_ragged(void) {
    const char *text = "a,b\r\nc\r\n";
    ScCsv *csv = sc_csv_parse(text, strlen(text), (ScCsvOpts){ 0 }, NULL);

    CHECK(strcmp(sc_csv_at(csv, 0, 1), "b") == 0,
          "parse: CRLF line endings strip the CR");
    CHECK(sc_csv_row_cols(csv, 1) == 1 && sc_csv_cols(csv) == 2,
          "parse: ragged rows keep their own field counts");

    sc_csv_free(csv);
}

static void check_tsv(void) {
    const char *text = "x\ty\n1\t2\n";
    ScCsv *csv = sc_csv_parse(
        text, strlen(text), (ScCsvOpts){ .delim = '\t' }, NULL
    );
    CHECK(strcmp(sc_csv_at(csv, 1, 0), "1") == 0
              && strcmp(sc_csv_at(csv, 1, 1), "2") == 0,
          "parse: tab delimiter splits TSV");
    sc_csv_free(csv);
}

static void check_header_lookup(void) {
    const char *text = "name,age\nAlice,30\nBob,25\n";
    ScCsv *csv = sc_csv_parse(
        text, strlen(text), (ScCsvOpts){ .has_header = true }, NULL
    );

    CHECK(sc_csv_has_header(csv), "header: has_header is reported");
    CHECK(strcmp(sc_csv_header(csv, 1), "age") == 0,
          "header: header label by column");
    CHECK(sc_csv_data_rows(csv) == 2,
          "header: data row count excludes the header");
    CHECK(strcmp(sc_csv_get(csv, 0, "name"), "Alice") == 0,
          "header: lookup by name in data row 0");
    CHECK(strcmp(sc_csv_get(csv, 1, "age"), "25") == 0,
          "header: lookup by name in data row 1");
    CHECK(sc_csv_get(csv, 0, "missing")[0] == '\0',
          "header: unknown column returns empty string");

    sc_csv_free(csv);
}


/* ── Writing ─────────────────────────────────────────────────────────────── */

static void check_writer_round_trip(void) {
    ScCsv *csv = sc_csv_new((ScCsvOpts){ 0 });
    sc_csv_add_row(csv, (const char *[]){ "name", "note" }, 2);
    sc_csv_add_row(csv, (const char *[]){ "a,b", "say \"hi\"" }, 2);
    sc_csv_add_row(csv, (const char *[]){ "plain", "x" }, 2);

    char *out = sc_csv_write(csv);
    const char *expected =
        "name,note\n"
        "\"a,b\",\"say \"\"hi\"\"\"\n"
        "plain,x\n";
    CHECK(out != NULL && strcmp(out, expected) == 0,
          "write: fields are quoted only when needed, with \"\" escaping");
    free(out);

    // The written document re-parses to the same cells.
    ScCsv *reparsed = sc_csv_parse(
        expected, strlen(expected), (ScCsvOpts){ 0 }, NULL
    );
    CHECK(strcmp(sc_csv_at(reparsed, 1, 1), "say \"hi\"") == 0,
          "write: output round-trips back through the reader");
    sc_csv_free(reparsed);

    sc_csv_free(csv);
}

static void check_unterminated_quote(void) {
    const char *text = "\"unterminated";
    ScParseError err = { 0 };
    ScCsv *csv = sc_csv_parse(text, strlen(text), (ScCsvOpts){ 0 }, &err);
    CHECK(csv == NULL && err.message[0] != '\0',
          "error: unterminated quoted field is rejected with a message");
}
