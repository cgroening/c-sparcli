// markdown_outline.c - split front matter, walk the heading outline, then
// edit the front matter from a value tree and re-serialize the document.
//
// Run (after `make`):
//   make run-example EX=c/data/markdown_outline

#include <serde/sparcli_serde.h>
#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_outline(const ScMdSection *section, int depth);

int main(void) {
    const char *doc =
        "+++\n"
        "title = \"Guide\"\n"
        "+++\n"
        "# Install\n"
        "Steps go here.\n"
        "## macOS\n"
        "brew install sparcli.\n"
        "# Usage\n"
        "Run it.\n";

    ScParseError err = { 0 };
    ScMarkdown *md = sc_markdown_parse(doc, strlen(doc), &err);
    if (!md) {
        sc_die(sc_parse_error_to_error(&err));
    }

    const ScValue *fm = sc_markdown_frontmatter(md);
    const char *title = fm ? sc_value_as_string(sc_value_get(fm, "title"))
                           : NULL;
    sc_markup_println("[bold]Outline[/]");
    printf("front matter title: %s\n", title ? title : "(none)");
    print_outline(sc_markdown_root(md), 0);

    // Edit the front matter through a value tree, then re-serialize: the new
    // block is written, not the original raw text.
    ScValue *meta = sc_value_object();
    sc_value_set(meta, "title", sc_value_string("Guide"));
    sc_value_set(meta, "version", sc_value_int(2));
    sc_markdown_set_frontmatter(md, SC_MD_FRONTMATTER_TOML, meta);
    sc_value_free(meta);

    char *out = sc_markdown_write(md);
    sc_rule_str(
        "rewritten",
        (ScRuleOpts){ .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE }
    );
    printf("%s", out ? out : "");
    free(out);

    sc_markdown_free(md);
    return 0;
}

// Prints each section's title, indented by depth, depth-first.
static void print_outline(const ScMdSection *section, int depth) {
    size_t count = sc_md_section_child_count(section);
    for (size_t i = 0; i < count; i++) {
        const ScMdSection *child = sc_md_section_child(section, i);
        for (int d = 0; d < depth; d++) {
            printf("  ");
        }
        printf("- %s\n", sc_md_section_title(child));
        print_outline(child, depth + 1);
    }
}
