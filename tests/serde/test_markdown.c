#include "test_serde.h"


// Forward declarations indented to reflect the call hierarchy.
static void check_frontmatter_toml(void);
static void check_frontmatter_yaml_raw(void);
static void check_no_frontmatter(void);
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
