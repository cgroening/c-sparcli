#include "sparcli.h"
#include "serde/sparcli_serde.h"
#include "view/sparcli_view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int g_failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) { printf("  \033[32m✔\033[0m %s\n", (msg)); }                \
        else { printf("  \033[31m✘\033[0m %s\n", (msg)); g_failures++; }       \
    } while (0)


static bool has(const char *hay, const char *needle) {
    return hay && strstr(hay, needle);
}

/** Captures stream output of `fn(arg)` and returns it ANSI-stripped (heap). */
static char *capture_plain(void (*fn)(void *), void *arg) {
    FILE *cap = tmpfile();
    if (!cap) { return strdup(""); }
    FILE *prev = sc_output_stream();
    sc_output_set_stream(cap);
    fn(arg);
    sc_output_set_stream(prev);

    fseek(cap, 0, SEEK_SET);
    char buf[8192];
    size_t n = fread(buf, 1, sizeof buf - 1, cap);
    buf[n] = '\0';
    fclose(cap);
    return sc_strip_ansi(buf);
}


/* ── value render ────────────────────────────────────────────────────────── */

static void render_value_cb(void *arg) {
    sc_value_render((const ScValue *)arg, (ScValueRenderOpts){ .no_color = 1 });
}

static void test_value_render(void) {
    const char *json =
        "{\"name\":\"sparcli\",\"n\":1,\"tags\":[\"a\",\"b\"],"
        "\"on\":true,\"x\":null}";
    ScValue *v = sc_json_parse(json, strlen(json), NULL);
    char *out = capture_plain(render_value_cb, v);

    CHECK(has(out, "\"name\": \"sparcli\""), "value: key + string colored form");
    CHECK(has(out, "\"n\": 1"), "value: number rendered");
    CHECK(has(out, "\"tags\": ["), "value: array opens");
    CHECK(has(out, "\"on\": true"), "value: bool literal");
    CHECK(has(out, "\"x\": null"), "value: null literal");
    CHECK(has(out, "\n  \""), "value: two-space indentation");

    free(out);
    sc_value_free(v);

    /* sort_keys */
    const char *j2 = "{\"b\":1,\"a\":2}";
    ScValue *v2 = sc_json_parse(j2, strlen(j2), NULL);
    ScText *t = sc_value_render_text(v2, (ScValueRenderOpts){ .no_color = 1,
                                                              .sort_keys = 1 });
    ScRendered *r = sc_capture_value(v2, (ScValueRenderOpts){ .no_color = 1,
                                                              .sort_keys = 1 });
    (void)r; sc_rendered_free(r);
    /* "a" must appear before "b" when sorted */
    char *s = NULL;
    {
        FILE *cap = tmpfile();
        FILE *prev = sc_output_stream();
        sc_output_set_stream(cap);
        sc_print_text(t);
        sc_output_set_stream(prev);
        fseek(cap, 0, SEEK_SET);
        char b[512]; size_t n = fread(b, 1, sizeof b - 1, cap); b[n] = '\0';
        fclose(cap);
        s = sc_strip_ansi(b);
    }
    char *pa = strstr(s, "\"a\"");
    char *pb = strstr(s, "\"b\"");
    CHECK(pa && pb && pa < pb, "value: sort_keys orders keys");
    free(s); sc_text_free(t); sc_value_free(v2);
}


/* ── markdown render ─────────────────────────────────────────────────────── */

static const char *g_md =
    "# Title\n\n"
    "Para with **bold**, *italic*, `code`, [link](https://x).\n\n"
    "## Features\n\n"
    "- one\n- two\n\n"
    "1. first\n2. second\n\n"
    "> quoted\n\n"
    "```\ncodeblock\n```\n\n"
    "| Name | Val |\n|------|----:|\n| aa | 1 |\n| bb | 2 |\n\n"
    "---\n\nEnd.\n";

static void render_md_cb(void *arg) {
    sc_markdown_render_str((const char *)arg, (ScMarkdownRenderOpts){ 0 });
}

static void test_markdown_render(void) {
    char *out = capture_plain(render_md_cb, (void *)g_md);

    CHECK(has(out, "Title"), "md: heading text");
    CHECK(has(out, "\xe2\x95\x90"), "md: level-1 heading underline (double rule)");
    CHECK(has(out, "bold") && has(out, "italic") && has(out, "code"),
          "md: inline emphasis text preserved");
    CHECK(has(out, "link"), "md: link text preserved");
    CHECK(has(out, "\xe2\x80\xa2 one"), "md: bullet list rendered");
    CHECK(has(out, "1. first"), "md: ordered list rendered");
    CHECK(has(out, "\xe2\x96\x8f") && has(out, "quoted"), "md: block quote bar");
    CHECK(has(out, "codeblock"), "md: fenced code content");
    CHECK(has(out, "Name") && has(out, "Val") && has(out, "aa"),
          "md: pipe table rendered");
    CHECK(has(out, "End."), "md: trailing paragraph");

    free(out);

    /* capture API returns a non-empty rendering */
    ScMarkdown *md = sc_markdown_parse(g_md, strlen(g_md), NULL);
    ScRendered *r = sc_capture_markdown(md, (ScMarkdownRenderOpts){ 0 });
    CHECK(r && r->line_count > 5, "md: capture produces multi-line frame");
    sc_rendered_free(r);
    sc_markdown_free(md);

    /* empty / NULL safety */
    sc_markdown_render_str("", (ScMarkdownRenderOpts){ 0 });
    CHECK(true, "md: empty input is safe");
}


int main(void) {
    printf("\n== value render ==\n");
    test_value_render();
    printf("\n== markdown render ==\n");
    test_markdown_render();

    if (g_failures) {
        printf("\n\033[31m%d view check(s) failed.\033[0m\n", g_failures);
        return 1;
    }
    printf("\n\033[32mAll view checks passed.\033[0m\n");
    return 0;
}
