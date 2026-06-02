#include "test_input.h"
#include "sparcli.h"
#include "core/sanitize_internal.h"
#include "internal.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


// Forward declarations indented to reflect call hierarchy
static void check_sanitize_copy(void);
static void check_mode_resolution(void);
static void check_widths(void);
    static int test_visible_width(const char *line);
static void check_text_and_markup(void);
static void check_strip_ansi(void);
static void check_print_sanitizes(void);
static void check_panel_alignment(void);
static void check_table_alignment(void);
    static bool all_lines_equal_width(const ScRendered *rendered);
    static bool rendered_contains(const ScRendered *rendered,
                                  const char *needle);
static bool str_eq(const char *actual, const char *expected);


/**
 * Pure unit tests for the ANSI-injection sanitizer: control-byte and
 * escape-sequence removal, the global/per-widget allow_ansi precedence,
 * ANSI-aware width calculation, and frame alignment with allowed ANSI.
 */
void test_sanitize(void) {
    // Every block restores the global default (allow_ansi = false)
    check_sanitize_copy();
    check_mode_resolution();
    check_widths();
    check_text_and_markup();
    check_strip_ansi();
    check_print_sanitizes();
    check_panel_alignment();
    check_table_alignment();
    sc_set_allow_ansi(false);
}


/* ── sc_sanitize_copy ────────────────────────────────────────────────────── */

static void check_sanitize_copy(void) {
    char *clean = sc_sanitize_copy("a\ab\bc\x0b\x7f" "d", false);
    CHECK(str_eq(clean, "abcd"),
          "sanitize: control bytes (BEL/BS/VT/DEL) are dropped");
    free(clean);

    clean = sc_sanitize_copy("line1\nline2\tend", false);
    CHECK(str_eq(clean, "line1\nline2\tend"),
          "sanitize: newline and tab are kept");
    free(clean);

    clean = sc_sanitize_copy("\033[31mred\033[0m", false);
    CHECK(str_eq(clean, "red"),
          "sanitize: CSI sequences stripped when disallowed");
    free(clean);

    clean = sc_sanitize_copy("\033[31mred\033[0m", true);
    CHECK(str_eq(clean, "\033[31mred\033[0m"),
          "sanitize: CSI sequences preserved when allowed");
    free(clean);

    clean = sc_sanitize_copy("a\033[31mb\ac", true);
    CHECK(str_eq(clean, "a\033[31mbc"),
          "sanitize: allow_ansi keeps CSI but still drops stray BEL");
    free(clean);

    clean = sc_sanitize_copy("\033]0;evil title\aX", false);
    CHECK(str_eq(clean, "X"),
          "sanitize: OSC (BEL-terminated) stripped when disallowed");
    free(clean);

    clean = sc_sanitize_copy("\033]8;;http://x\033\\link\033]8;;\033\\", true);
    CHECK(str_eq(clean, "\033]8;;http://x\033\\link\033]8;;\033\\"),
          "sanitize: OSC hyperlink (ST-terminated) preserved when allowed");
    free(clean);

    clean = sc_sanitize_copy("\033Pq#0\033\\after", false);
    CHECK(str_eq(clean, "after"),
          "sanitize: DCS string sequence stripped");
    free(clean);

    clean = sc_sanitize_copy("\033c reset", false);
    CHECK(str_eq(clean, " reset"),
          "sanitize: two-char ESC sequence (RIS) stripped");
    free(clean);

    clean = sc_sanitize_copy("end\033", false);
    CHECK(str_eq(clean, "end"),
          "sanitize: trailing lone ESC dropped");
    free(clean);

    clean = sc_sanitize_copy("a\033\033[1mb", true);
    CHECK(str_eq(clean, "a\033[1mb"),
          "sanitize: malformed ESC dropped, following valid CSI kept");
    free(clean);

    clean = sc_sanitize_copy("x\033[31", false);
    CHECK(str_eq(clean, "x[31"),
          "sanitize: unterminated CSI drops only the ESC byte");
    free(clean);

    // The path the external editor read-back uses (never allows ANSI)
    clean = sc_sanitize_copy("safe\r\033[2Jrm -rf\x07", false);
    CHECK(str_eq(clean, "saferm -rf"),
          "sanitize: editor read-back removes CR, clear-screen and BEL");
    free(clean);
}


/* ── Global / per-widget mode resolution ─────────────────────────────────── */

static void check_mode_resolution(void) {
    sc_set_allow_ansi(false);
    CHECK(!sc_allow_ansi(), "mode: global default is disallow");
    CHECK(!sc_ansi_mode_resolve(SC_ANSI_MODE_DEFAULT),
          "mode: DEFAULT follows global (off)");
    CHECK(sc_ansi_mode_resolve(SC_ANSI_MODE_ALLOW),
          "mode: ALLOW overrides global off");

    sc_set_allow_ansi(true);
    CHECK(sc_allow_ansi(), "mode: setter switches the global on");
    CHECK(sc_ansi_mode_resolve(SC_ANSI_MODE_DEFAULT),
          "mode: DEFAULT follows global (on)");
    CHECK(!sc_ansi_mode_resolve(SC_ANSI_MODE_SANITIZE),
          "mode: SANITIZE overrides global on");

    sc_set_allow_ansi(false);
}


/* ── ANSI-aware width calculation ────────────────────────────────────────── */

static void check_widths(void) {
    const char *colored = "\033[31mABC\033[0m";
    CHECK(sc_utf8_string_length(colored, strlen(colored)) == 3,
          "width: CSI sequences count zero columns");

    const char *osc = "\033]0;title\aAB";
    CHECK(sc_utf8_string_length(osc, strlen(osc)) == 2,
          "width: OSC sequences count zero columns");

    const char *umlauts = "\303\204\303\226\303\234";   /* ÄÖÜ */
    CHECK(sc_utf8_string_length(umlauts, strlen(umlauts)) == 3,
          "width: UTF-8 codepoints still count one column each");

    const char *bell = "a\ab";
    CHECK(sc_utf8_string_length(bell, strlen(bell)) == 2,
          "width: sanitizer-dropped control bytes count zero columns");

    // Trimming never cuts inside an escape sequence
    const char *long_colored = "\033[31mABCDEF\033[0m";
    size_t kept_bytes = sc_utf8_trim_to_cols(long_colored, 3);
    CHECK(kept_bytes == strlen("\033[31m") + 3,
          "width: trim keeps the full leading escape sequence + 3 columns");
}

/**
 * Test-local visible width: skips `ESC … letter` sequences and UTF-8
 * continuation bytes. Independent of the library implementation so the
 * alignment checks do not rely on the code under test.
 */
static int test_visible_width(const char *line) {
    int width = 0;
    const unsigned char *cursor = (const unsigned char *)line;
    while (*cursor) {
        if (*cursor == 0x1b) {
            cursor++;
            if (*cursor == '[' || *cursor == ']') {
                cursor++;
                while (*cursor && *cursor != 'm' && *cursor != '\a'
                       && *cursor != 0x1b) {
                    cursor++;
                }
                if (*cursor == 0x1b && cursor[1] == '\\') { cursor++; }
                if (*cursor) { cursor++; }
            }
            continue;
        }
        if ((*cursor & 0xC0) != 0x80) { width++; }
        cursor++;
    }
    return width;
}


/* ── ScText / markup sanitization ────────────────────────────────────────── */

static void check_text_and_markup(void) {
    sc_set_allow_ansi(false);

    ScText *text = sc_text_new();
    sc_text_append(text, "\033[31minjected\033[0m", (ScTextStyle){ 0 });
    ScSpan span = sc_text_span(text, 0);
    CHECK(str_eq(span.raw_str, "injected"),
          "text: sc_text_append strips ANSI by default");
    sc_text_free(text);

    sc_set_allow_ansi(true);
    text = sc_text_new();
    sc_text_append(text, "\033[31mok\033[0m", (ScTextStyle){ 0 });
    span = sc_text_span(text, 0);
    CHECK(str_eq(span.raw_str, "\033[31mok\033[0m"),
          "text: sc_text_append keeps ANSI when globally allowed");
    CHECK(sc_text_visible_width(text) == 2,
          "text: visible width skips allowed ANSI sequences");
    sc_text_free(text);
    sc_set_allow_ansi(false);

    // Markup: raw ESC bytes in the text are stripped by default
    ScText *parsed = sc_markup_parse("[bold]hi\033[31m there[/]");
    bool has_escape = false;
    for (size_t i = 0; i < sc_text_span_count(parsed); i++) {
        if (strchr(sc_text_span(parsed, i).raw_str, '\033')) {
            has_escape = true;
        }
    }
    CHECK(!has_escape, "markup: raw ESC bytes in text chunks are stripped");
    sc_text_free(parsed);

    // Markup opts: per-parse ANSI passthrough
    parsed = sc_markup_parse_opts(
        "x\033[31my",
        (ScMarkupOpts){ .ansi = SC_ANSI_MODE_ALLOW }
    );
    span = sc_text_span(parsed, 0);
    CHECK(str_eq(span.raw_str, "x\033[31my"),
          "markup: opts.ansi = ALLOW keeps raw escape sequences");
    sc_text_free(parsed);
}


/* ── Hardened sc_strip_ansi ──────────────────────────────────────────────── */

static void check_strip_ansi(void) {
    char *stripped = sc_strip_ansi(
        "\033]0;title\a\033[31mtext\033[0m\033Pq\033\\\033c\033"
    );
    CHECK(str_eq(stripped, "text"),
          "strip_ansi: removes OSC, CSI, DCS, two-char ESC and lone ESC");
    free(stripped);

    stripped = sc_strip_ansi("a\tb\nc");
    CHECK(str_eq(stripped, "a\tb\nc"),
          "strip_ansi: keeps non-escape bytes (tab, newline)");
    free(stripped);

    stripped = sc_strip_ansi("plain");
    CHECK(str_eq(stripped, "plain"), "strip_ansi: plain text unchanged");
    free(stripped);
}


/* ── sc_print sanitizes by default ───────────────────────────────────────── */

static void check_print_sanitizes(void) {
    sc_set_allow_ansi(false);

    FILE *capture = tmpfile();
    if (!capture) {
        CHECK(false, "print: tmpfile() for capture available");
        return;
    }
    sc_output_set_stream(capture);
    sc_print("\033]0;evil\a\033[31mhello", (ScTextStyle){ 0 });
    sc_output_set_stream(NULL);

    char buffer[256] = { 0 };
    if (fseek(capture, 0, SEEK_SET) != 0) {
        fclose(capture);
        CHECK(false, "print: rewinding the capture stream succeeds");
        return;
    }
    size_t read_bytes = fread(buffer, 1, sizeof(buffer) - 1, capture);
    buffer[read_bytes] = '\0';
    fclose(capture);

    // The reset emitted by sc_print itself is expected; the injected
    // OSC/CSI sequences must be gone.
    CHECK(strstr(buffer, "\033]0;") == NULL
              && strstr(buffer, "\033[31m") == NULL
              && strstr(buffer, "hello") != NULL,
          "print: sc_print strips injected escape sequences by default");
}


/* ── Frame alignment with allowed ANSI ───────────────────────────────────── */

static void check_panel_alignment(void) {
    ScPanelOpts boxed = { .border.type = SC_BORDER_SINGLE };

    // Default: injected ANSI is stripped, borders aligned
    sc_set_allow_ansi(false);
    ScRendered *rendered = sc_capture_panel_str(
        "\033[31mevil\033[0m content", boxed
    );
    CHECK(rendered && !rendered_contains(rendered, "\033[31m"),
          "panel: strips injected ANSI by default");
    CHECK(rendered && all_lines_equal_width(rendered),
          "panel: borders aligned after stripping");
    sc_rendered_free(rendered);

    // Global allow: ANSI kept, borders still aligned
    sc_set_allow_ansi(true);
    rendered = sc_capture_panel_str("\033[31mHELLO\033[0m world", boxed);
    CHECK(rendered && rendered_contains(rendered, "\033[31m"),
          "panel: keeps ANSI when globally allowed");
    CHECK(rendered && all_lines_equal_width(rendered),
          "panel: borders aligned with embedded ANSI (global allow)");
    sc_rendered_free(rendered);
    sc_set_allow_ansi(false);

    // Per-widget ALLOW overrides global off; frames stay aligned
    ScPanelOpts allow_opts = boxed;
    allow_opts.ansi = SC_ANSI_MODE_ALLOW;
    rendered = sc_capture_panel_str("\033[32mGREEN\033[0m", allow_opts);
    CHECK(rendered && rendered_contains(rendered, "\033[32m"),
          "panel: opts.ansi = ALLOW overrides global off");
    CHECK(rendered && all_lines_equal_width(rendered),
          "panel: borders aligned with per-widget allow");
    sc_rendered_free(rendered);

    // Per-widget SANITIZE overrides global on
    sc_set_allow_ansi(true);
    ScPanelOpts sanitize_opts = boxed;
    sanitize_opts.ansi = SC_ANSI_MODE_SANITIZE;
    rendered = sc_capture_panel_str("\033[32mGREEN\033[0m", sanitize_opts);
    CHECK(rendered && !rendered_contains(rendered, "\033[32m"),
          "panel: opts.ansi = SANITIZE overrides global on");
    sc_rendered_free(rendered);
    sc_set_allow_ansi(false);

    // Titles are sanitized too
    ScPanelOpts titled = boxed;
    titled.title.text = "\033[31mTitle\033[0m";
    rendered = sc_capture_panel_str("content", titled);
    CHECK(rendered && !rendered_contains(rendered, "\033[31m"),
          "panel: title string is sanitized by default");
    CHECK(rendered && all_lines_equal_width(rendered),
          "panel: borders aligned with sanitized title");
    sc_rendered_free(rendered);
}

static void check_table_alignment(void) {
    sc_set_allow_ansi(false);

    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "Col A", (ScColOpts){ 0 });
    sc_table_add_column(table, "Col B", (ScColOpts){ 0 });
    sc_table_add_row(table, (ScCell[]){
        sc_cell("\033[31mRED\033[0m"), sc_cell("plain"),
    }, 2);

    ScTableOpts bordered = {
        .border.type = SC_BORDER_SINGLE,
        .header.row = true,
    };

    // Default: cell ANSI stripped, borders aligned
    ScRendered *rendered = sc_capture_table(table, bordered);
    CHECK(rendered && !rendered_contains(rendered, "\033[31m"),
          "table: strips injected cell ANSI by default");
    CHECK(rendered && all_lines_equal_width(rendered),
          "table: borders aligned after stripping");
    sc_rendered_free(rendered);

    // Per-widget ALLOW: ANSI kept, borders still aligned
    ScTableOpts allow_opts = bordered;
    allow_opts.ansi = SC_ANSI_MODE_ALLOW;
    rendered = sc_capture_table(table, allow_opts);
    CHECK(rendered && rendered_contains(rendered, "\033[31m"),
          "table: opts.ansi = ALLOW keeps cell ANSI");
    CHECK(rendered && all_lines_equal_width(rendered),
          "table: borders aligned with embedded ANSI");
    sc_rendered_free(rendered);

    sc_table_free(table);
}

/** Returns `true` when every rendered line has the same visible width. */
static bool all_lines_equal_width(const ScRendered *rendered) {
    if (!rendered || rendered->line_count == 0) { return false; }
    int first_width = test_visible_width(rendered->lines[0]);
    for (size_t i = 1; i < rendered->line_count; i++) {
        if (test_visible_width(rendered->lines[i]) != first_width) {
            return false;
        }
    }
    return true;
}

/** Returns `true` when any rendered line contains `needle`. */
static bool rendered_contains(const ScRendered *rendered,
                              const char *needle) {
    if (!rendered) { return false; }
    for (size_t i = 0; i < rendered->line_count; i++) {
        if (strstr(rendered->lines[i], needle)) { return true; }
    }
    return false;
}

/** NULL-safe string equality. */
static bool str_eq(const char *actual, const char *expected) {
    return actual != NULL && strcmp(actual, expected) == 0;
}
