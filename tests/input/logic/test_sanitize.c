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
static void check_links(void);
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
    check_links();
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

    /* Display-width awareness: East-Asian Wide / Fullwidth / emoji = 2,
       combining marks = 0, ambiguous symbols (sparcli's own glyphs) = 1. */
    const char *cjk = "\344\275\240\345\245\275";       /* 你好 (2 CJK) */
    CHECK(sc_utf8_string_length(cjk, strlen(cjk)) == 4,
          "width: CJK ideographs count two columns each");
    const char *emoji = "\360\237\216\211";              /* 🎉 U+1F389 */
    CHECK(sc_utf8_string_length(emoji, strlen(emoji)) == 2,
          "width: emoji count two columns");
    const char *fullwidth = "\357\274\241";              /* Ａ U+FF21 */
    CHECK(sc_utf8_string_length(fullwidth, strlen(fullwidth)) == 2,
          "width: fullwidth forms count two columns");
    const char *combining = "e\314\201";                 /* e + U+0301 */
    CHECK(sc_utf8_string_length(combining, strlen(combining)) == 1,
          "width: combining marks count zero columns");
    const char *checkmark = "\342\234\224";              /* ✔ U+2714 */
    CHECK(sc_utf8_string_length(checkmark, strlen(checkmark)) == 1,
          "width: ambiguous symbols stay one column");

    /* trim never splits a wide glyph: 你好 at 3 cols keeps only 你 (2 cols,
       3 bytes), stopping a column short; at 4 cols both fit (6 bytes). */
    CHECK(sc_utf8_trim_to_cols(cjk, 3) == 3,
          "width: trim keeps a whole CJK glyph and stops a column short");
    CHECK(sc_utf8_trim_to_cols(cjk, 4) == 6,
          "width: trim fits both CJK glyphs at exactly four columns");

    /* The public sc_text_visible_width is display-width aware too. */
    ScText *cjk_text = sc_text_new();
    sc_text_append(cjk_text, cjk, (ScTextStyle){ 0 });
    CHECK(sc_text_visible_width(cjk_text) == 4,
          "width: ScText visible width counts CJK as two columns");
    sc_text_free(cjk_text);

    const char *bell = "a\ab";
    CHECK(sc_utf8_string_length(bell, strlen(bell)) == 2,
          "width: sanitizer-dropped control bytes count zero columns");

    // Trimming never cuts inside an escape sequence
    const char *long_colored = "\033[31mABCDEF\033[0m";
    size_t kept_bytes = sc_utf8_trim_to_cols(long_colored, 3);
    CHECK(kept_bytes == strlen("\033[31m") + 3,
          "width: trim keeps the full leading escape sequence + 3 columns");

    // Truncated UTF-8 at the end of the string must never be walked past
    // (regression: fuzz_args found an out-of-bounds read here)
    const char *truncated_lead = "ab\xe2";          /* 3-byte lead, no tail */
    CHECK(sc_utf8_trim_to_cols(truncated_lead, 10) == 3,
          "width: trim stops at the NUL after a truncated UTF-8 lead byte");
    CHECK(sc_utf8_string_length(truncated_lead, 3) == 3,
          "width: length never counts past a truncated UTF-8 lead byte");

    const char *truncated_four = "x\xf0\x9f";       /* 4-byte lead, 1 tail */
    CHECK(sc_utf8_trim_to_cols(truncated_four, 10) == 3,
          "width: trim is bounded for truncated 4-byte sequences");

    // A panel rendering a wrapped truncated-UTF-8 string must not crash
    ScRendered *rendered = sc_capture_panel_str(
        "word \xe2\x82 another-very-long-word-that-wraps \xf0",
        (ScPanelOpts){ .border.type = SC_BORDER_SINGLE, .width = 16 }
    );
    CHECK(rendered != NULL,
          "width: word-wrap survives truncated UTF-8 in the content");
    sc_rendered_free(rendered);

    // A long unbroken double-width (CJK) run must hard-break across lines, not
    // loop forever or drop glyphs when a wide glyph won't fit the last column.
    ScRendered *cjk_wrap = sc_capture_panel_str(
        "\344\275\240\345\245\275\344\275\240\345\245\275\344\275\240"
        "\345\245\275\344\275\240\345\245\275\344\275\240\345\245\275", /* 你好x5 */
        (ScPanelOpts){ .border.type = SC_BORDER_SINGLE, .width = 9 }
    );
    CHECK(cjk_wrap && cjk_wrap->line_count >= 4,
          "width: a long CJK run hard-breaks across multiple lines");
    sc_rendered_free(cjk_wrap);
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


/* ── OSC-8 hyperlinks ────────────────────────────────────────────────────── */

static void check_links(void) {
    sc_set_allow_ansi(false);

    // URL scrubbing: control bytes, ESC sequences and non-ASCII removed
    char *scrubbed = sc_osc8_scrub_url("https://x.test/\033[31m\a\n\tpath");
    CHECK(str_eq(scrubbed, "https://x.test/[31mpath"),
          "links: URL scrubbing drops ESC, BEL, newline and tab bytes");
    free(scrubbed);

    scrubbed = sc_osc8_scrub_url("\033\a\x01");
    CHECK(str_eq(scrubbed, ""),
          "links: URL of only control bytes scrubs to empty");
    free(scrubbed);

    // sc_osc8_wrap builds the full OSC-8 sequence
    char *wrapped = sc_osc8_wrap("text", "https://x.test");
    CHECK(str_eq(wrapped,
                 "\033]8;;https://x.test\033\\text\033]8;;\033\\"),
          "links: sc_osc8_wrap builds opener + text + closer");
    free(wrapped);

    // sc_text_append_link: visible width counts only the link text
    ScText *text = sc_text_new();
    sc_text_append_link(
        text, "click", "https://example.com", (ScTextStyle){ 0 }
    );
    CHECK(sc_text_visible_width(text) == 5,
          "links: OSC-8 escape bytes occupy zero visible columns");
    CHECK(sc_text_span_count(text) == 1,
          "links: link is stored as a single span");
    sc_text_free(text);

    // NULL / empty URL degrades to a plain span without escape bytes
    text = sc_text_new();
    sc_text_append_link(text, "plain", NULL, (ScTextStyle){ 0 });
    sc_text_append_link(text, " more", "", (ScTextStyle){ 0 });
    ScSpan span = sc_text_span(text, 0);
    CHECK(span.raw_str && strchr(span.raw_str, '\033') == NULL,
          "links: NULL URL appends a plain span");
    span = sc_text_span(text, 1);
    CHECK(span.raw_str && strchr(span.raw_str, '\033') == NULL,
          "links: empty URL appends a plain span");
    sc_text_free(text);

    // The visible link text crosses the trust boundary (sanitized)
    text = sc_text_new();
    sc_text_append_link(
        text, "evil\033[31mtext", "https://x.test", (ScTextStyle){ 0 }
    );
    span = sc_text_span(text, 0);
    CHECK(span.raw_str && strstr(span.raw_str, "\033[31m") == NULL,
          "links: injected CSI in the link text is stripped");
    CHECK(span.raw_str && strstr(span.raw_str, "eviltext") != NULL,
          "links: visible link text is kept after sanitizing");
    sc_text_free(text);

    // Plain sc_text_append can never produce OSC-8 bytes
    text = sc_text_new();
    sc_text_append(
        text, "\033]8;;https://evil\033\\x\033]8;;\033\\", (ScTextStyle){ 0 }
    );
    span = sc_text_span(text, 0);
    CHECK(span.raw_str && strchr(span.raw_str, '\033') == NULL,
          "links: sc_text_append strips injected OSC-8 sequences");
    sc_text_free(text);

    // Markup [link=URL] produces one OSC-8 wrapped span
    ScText *parsed = sc_markup_parse(
        "[link=https://example.com]docs[/link]"
    );
    CHECK(sc_text_span_count(parsed) == 1,
          "links: markup link parses to a single span");
    span = sc_text_span(parsed, 0);
    CHECK(span.raw_str
              && strstr(span.raw_str, "\033]8;;https://example.com\033\\")
                     != NULL
              && strstr(span.raw_str, "docs") != NULL,
          "links: markup link span contains opener, text and closer");
    CHECK(sc_text_visible_width(parsed) == 4,
          "links: markup link width counts only the visible text");
    sc_text_free(parsed);

    // Markup link body is literal: nested tags stay verbatim
    parsed = sc_markup_parse("[link=https://x.test]a [bold]b[/bold][/link]");
    span = sc_text_span(parsed, 0);
    CHECK(span.raw_str && strstr(span.raw_str, "[bold]") != NULL,
          "links: markup link body keeps nested tags verbatim");
    sc_text_free(parsed);

    // Markup link URL is scrubbed
    parsed = sc_markup_parse("[link=https://x.test/\033[31mevil]e[/link]");
    span = sc_text_span(parsed, 0);
    CHECK(span.raw_str && strstr(span.raw_str, "\033[31m") == NULL,
          "links: markup link URL is scrubbed of escape sequences");
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

    /* A double-width (CJK) cell in a fixed-width column must truncate AND pad so
       the border stays aligned. Measured with the display-width-aware library
       width (the local codepoint counter would mis-measure wide glyphs): every
       rendered line ends up the same number of terminal columns. */
    ScTableData *cjk_table = sc_table_new();
    sc_table_add_column(cjk_table, "Name", (ScColOpts){ .fixed_width = 6 });
    sc_table_add_row(cjk_table, (ScCell[]){   /* 你好你好 = 8 cols, must clip */
        sc_cell("\344\275\240\345\245\275\344\275\240\345\245\275"),
    }, 1);
    ScRendered *cjk_r = sc_capture_table(cjk_table,
        (ScTableOpts){ .border.type = SC_BORDER_SINGLE });
    bool cjk_aligned = cjk_r != NULL && cjk_r->line_count > 0;
    if (cjk_aligned) {
        size_t w0 = sc_utf8_string_length(cjk_r->lines[0],
                                          strlen(cjk_r->lines[0]));
        for (size_t i = 1; i < cjk_r->line_count; i++) {
            if (sc_utf8_string_length(cjk_r->lines[i],
                                      strlen(cjk_r->lines[i])) != w0) {
                cjk_aligned = false;
            }
        }
    }
    CHECK(cjk_aligned,
          "table: a truncated CJK cell pads to keep the borders aligned");
    sc_rendered_free(cjk_r);
    sc_table_free(cjk_table);
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
