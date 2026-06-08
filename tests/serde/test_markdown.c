#include "test_serde.h"


// Forward declarations indented to reflect the call hierarchy.
static void check_frontmatter_toml(void);
static void check_frontmatter_yaml_raw(void);
static void check_no_frontmatter(void);
static void check_frontmatter_malformed(void);
static void check_section_tree(void);
static void check_find_and_body(void);
static void check_code_fence_not_heading(void);
static void check_build_and_write(void);
static void check_set_frontmatter(void);
static void check_round_trip(void);


/** Exercises front-matter splitting and the section tree. */
void test_serde_markdown(void) {
    check_frontmatter_toml();
    check_frontmatter_yaml_raw();
    check_no_frontmatter();
    check_frontmatter_malformed();
    check_section_tree();
    check_find_and_body();
    check_code_fence_not_heading();
    check_build_and_write();
    check_set_frontmatter();
    check_round_trip();
}


/* ── Front matter ────────────────────────────────────────────────────────── */

static void check_frontmatter_toml(void) {
    const char *doc =
        "+++\n"
        "title = \"Post\"\n"
        "draft = false\n"
        "+++\n"
        "# Hello\n"
        "Body text.\n";
    ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), NULL);

    CHECK(sc_markdown_frontmatter_format(md) == SC_MD_FRONTMATTER_TOML,
          "frontmatter: +++ detected as TOML");
    const ScValue *fm = sc_markdown_frontmatter(md);
    CHECK(fm != NULL
              && strcmp(sc_value_as_string(sc_value_get(fm, "title")), "Post")
                     == 0,
          "frontmatter: TOML parsed into ScValue");
    CHECK(strncmp(sc_markdown_body(md), "# Hello", 7) == 0,
          "frontmatter: body starts after the closing fence");

    sc_markdown_free(md);
}

static void check_frontmatter_yaml_raw(void) {
    const char *doc =
        "---\n"
        "title: Post\n"
        "---\n"
        "Body.\n";
    ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), NULL);

    CHECK(sc_markdown_frontmatter_format(md) == SC_MD_FRONTMATTER_YAML,
          "frontmatter: --- detected as YAML");
    CHECK(strcmp(sc_markdown_frontmatter_raw(md), "title: Post") == 0,
          "frontmatter: YAML raw block captured");
    const ScValue *fm = sc_markdown_frontmatter(md);
    CHECK(fm != NULL
              && strcmp(sc_value_as_string(sc_value_get(fm, "title")), "Post")
                     == 0,
          "frontmatter: YAML parsed into ScValue");

    sc_markdown_free(md);
}

static void check_no_frontmatter(void) {
    const char *doc = "# Title\nText.\n";
    ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), NULL);
    CHECK(sc_markdown_frontmatter_format(md) == SC_MD_FRONTMATTER_NONE,
          "frontmatter: absent when the document starts with content");
    CHECK(strcmp(sc_markdown_frontmatter_raw(md), "") == 0,
          "frontmatter: raw is empty when absent");
    CHECK(!sc_markdown_frontmatter_malformed(md),
          "frontmatter: not malformed when absent");
    CHECK(sc_markdown_frontmatter_error(md) == NULL,
          "frontmatter: no error when absent");
    sc_markdown_free(md);
}

/**
 * A present-but-broken front-matter block must be distinguishable from no block:
 * the parse still succeeds (body usable), but `frontmatter` is NULL while
 * `frontmatter_malformed` is true and the sub-parse error is exposed.
 */
static void check_frontmatter_malformed(void) {
    // Malformed YAML: an unterminated flow collection.
    const char *yaml_doc =
        "---\n"
        "[unterminated\n"
        "---\n"
        "Body.\n";
    ScMarkdown *md = sc_markdown_parse(yaml_doc, strlen(yaml_doc), NULL);
    CHECK(md != NULL, "malformed: document still parses (lenient)");
    CHECK(sc_markdown_frontmatter_format(md) == SC_MD_FRONTMATTER_YAML,
          "malformed: YAML fence still detected");
    CHECK(sc_markdown_frontmatter(md) == NULL,
          "malformed: no parsed value for a broken block");
    CHECK(sc_markdown_frontmatter_malformed(md),
          "malformed: flag set for a present-but-broken block");
    const ScParseError *err = sc_markdown_frontmatter_error(md);
    CHECK(err != NULL && err->message[0] != '\0',
          "malformed: sub-parse error is exposed with a message");
    CHECK(strncmp(sc_markdown_body(md), "Body.", 5) == 0,
          "malformed: body is still recovered");
    sc_markdown_free(md);

    // No front matter: not malformed, no error.
    const char *plain = "# Title\nText.\n";
    md = sc_markdown_parse(plain, strlen(plain), NULL);
    CHECK(!sc_markdown_frontmatter_malformed(md)
              && sc_markdown_frontmatter_error(md) == NULL,
          "malformed: absent block is not flagged");
    sc_markdown_free(md);

    // Valid front matter: not malformed, no error.
    const char *valid =
        "+++\n"
        "title = \"Post\"\n"
        "+++\n"
        "Body.\n";
    md = sc_markdown_parse(valid, strlen(valid), NULL);
    CHECK(!sc_markdown_frontmatter_malformed(md)
              && sc_markdown_frontmatter_error(md) == NULL
              && sc_markdown_frontmatter(md) != NULL,
          "malformed: valid block is not flagged");
    sc_markdown_free(md);

    // Line offset: the error line points into the original document, past the
    // opening fence. The broken TOML line is document line 3.
    const char *toml_doc =
        "+++\n"
        "title = \"Post\"\n"
        "bad line here\n"
        "+++\n"
        "Body.\n";
    md = sc_markdown_parse(toml_doc, strlen(toml_doc), NULL);
    const ScParseError *terr = sc_markdown_frontmatter_error(md);
    CHECK(terr != NULL && terr->line == 3,
          "malformed: error line is offset to the document line");
    sc_markdown_free(md);

    // set_frontmatter_raw must flag a broken block, then clear on a valid one.
    md = sc_markdown_new();
    sc_markdown_set_frontmatter_raw(md, SC_MD_FRONTMATTER_YAML, "[unterminated");
    CHECK(sc_markdown_frontmatter_malformed(md),
          "malformed: set_frontmatter_raw flags a broken block");
    sc_markdown_set_frontmatter_raw(md, SC_MD_FRONTMATTER_YAML, "title: ok");
    CHECK(!sc_markdown_frontmatter_malformed(md)
              && sc_markdown_frontmatter_error(md) == NULL,
          "malformed: a later valid block clears the flag");
    sc_markdown_free(md);
}


/* ── Section tree ────────────────────────────────────────────────────────── */

static void check_section_tree(void) {
    const char *doc =
        "Preamble line.\n"
        "# Install\n"
        "Intro.\n"
        "## macOS\n"
        "Brew it.\n"
        "## Linux\n"
        "Apt it.\n"
        "# Usage\n"
        "Run it.\n";
    ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), NULL);
    ScMdSection *root = sc_markdown_root(md);

    CHECK(strcmp(sc_md_section_body(root), "Preamble line.") == 0,
          "tree: root body holds the preamble");
    CHECK(sc_md_section_child_count(root) == 2,
          "tree: two top-level sections");

    ScMdSection *install = sc_md_section_child(root, 0);
    CHECK(sc_md_section_level(install) == 1
              && strcmp(sc_md_section_title(install), "Install") == 0,
          "tree: first heading is level-1 Install");
    CHECK(sc_md_section_child_count(install) == 2,
          "tree: Install has two sub-sections");
    CHECK(strcmp(sc_md_section_title(sc_md_section_child(install, 1)), "Linux")
              == 0,
          "tree: nested section title");

    sc_markdown_free(md);
}

static void check_find_and_body(void) {
    const char *doc =
        "# A\n## B\n### C\nLeaf body.\n# D\n";
    ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), NULL);
    ScMdSection *root = sc_markdown_root(md);

    ScMdSection *c = sc_md_section_find(root, "A/B/C");
    CHECK(c != NULL && sc_md_section_level(c) == 3,
          "find: resolves a nested heading path");
    CHECK(c != NULL && strcmp(sc_md_section_body(c), "Leaf body.") == 0,
          "find: section body is the text beneath the heading");
    CHECK(sc_md_section_find(root, "A/missing") == NULL,
          "find: unknown path returns NULL");

    sc_markdown_free(md);
}

static void check_code_fence_not_heading(void) {
    const char *doc =
        "# Real\n"
        "```\n"
        "# not a heading\n"
        "```\n"
        "After.\n";
    ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), NULL);
    ScMdSection *root = sc_markdown_root(md);

    CHECK(sc_md_section_child_count(root) == 1,
          "fence: a # inside a code fence is not a heading");
    ScMdSection *real = sc_md_section_child(root, 0);
    CHECK(strstr(sc_md_section_body(real), "# not a heading") != NULL,
          "fence: fenced content is kept in the section body");

    sc_markdown_free(md);
}


/* ── Building / writing ──────────────────────────────────────────────────── */

static void check_build_and_write(void) {
    ScMarkdown *md = sc_markdown_new();
    ScMdSection *root = sc_markdown_root(md);
    ScMdSection *intro = sc_md_section_add(root, 1, "Intro");
    sc_md_section_set_body(intro, "Welcome.");
    sc_md_section_add(intro, 2, "Details");

    char *out = sc_markdown_write(md);
    const char *expected =
        "# Intro\n"
        "\n"
        "Welcome.\n"
        "\n"
        "## Details\n";
    CHECK(out != NULL && strcmp(out, expected) == 0,
          "write: heading + body + nested heading layout");
    free(out);
    sc_markdown_free(md);
}

static void check_set_frontmatter(void) {
    // Build a document, set its front matter from a value tree, and confirm
    // the serialized block is emitted and re-parses.
    ScMarkdown *md = sc_markdown_new();
    sc_md_section_set_body(sc_markdown_root(md), "Body text.");

    ScValue *meta = sc_value_object();
    sc_value_set(meta, "title", sc_value_string("Generated"));
    sc_value_set(meta, "draft", sc_value_bool(false));
    bool ok = sc_markdown_set_frontmatter(md, SC_MD_FRONTMATTER_TOML, meta);
    sc_value_free(meta);

    char *out = sc_markdown_write(md);
    ScMarkdown *again = sc_markdown_parse(out, strlen(out), NULL);
    const ScValue *fm = sc_markdown_frontmatter(again);
    CHECK(ok && fm != NULL
              && strcmp(sc_value_as_string(sc_value_get(fm, "title")),
                        "Generated") == 0,
          "set_frontmatter: value tree is serialized and re-parses on write");
    CHECK(strncmp(out, "+++\n", 4) == 0,
          "set_frontmatter: TOML front matter is fenced with +++");

    free(out);
    sc_markdown_free(again);
    sc_markdown_free(md);
}

static void check_round_trip(void) {
    const char *doc =
        "+++\n"
        "k = 1\n"
        "+++\n"
        "# Title\n"
        "Body.\n"
        "## Sub\n"
        "More.\n";
    ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), NULL);
    char *out = sc_markdown_write(md);

    // Re-parse the output and confirm the structure survived.
    ScMarkdown *again = sc_markdown_parse(out, strlen(out), NULL);
    ScMdSection *sub = sc_md_section_find(sc_markdown_root(again), "Title/Sub");
    CHECK(again != NULL
              && sc_markdown_frontmatter_format(again) == SC_MD_FRONTMATTER_TOML
              && sub != NULL && strcmp(sc_md_section_body(sub), "More.") == 0,
          "round trip: frontmatter + nested section survive parse->write->parse");

    free(out);
    sc_markdown_free(again);
    sc_markdown_free(md);
}
