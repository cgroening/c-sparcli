#include "serde/sparcli_serde.h"
#include "fuzz_common.h"

#include <stdlib.h>

/**
 * Fuzz target for the Markdown front-matter splitter and section-tree parser:
 * arbitrary bytes must never crash, leak or trigger UB. Any document it builds
 * is walked (sections + bodies) and round-tripped through the writer.
 */
static void walk(ScMdSection *section) {
    sc_md_section_level(section);
    sc_md_section_title(section);
    sc_md_section_body(section);
    for (size_t i = 0; i < sc_md_section_child_count(section); i++) {
        walk(sc_md_section_child(section, i));
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ScParseError err = { 0 };
    ScMarkdown *md = sc_markdown_parse((const char *)data, size, &err);
    if (!md) {
        return 0;
    }

    sc_markdown_frontmatter_raw(md);
    sc_markdown_frontmatter(md);
    sc_markdown_body(md);
    walk(sc_markdown_root(md));
    free(sc_markdown_write(md));

    sc_markdown_free(md);
    return 0;
}
