#pragma once

#include "core/sparcli_core.h"
#include "core/sparcli_rendered.h"
#include "core/sparcli_export.h"
#include "serde/sparcli_markdown.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_markdown_render.h
 * @brief Render Markdown to the terminal through the sparcli widget stack.
 *
 * Walks a parsed `ScMarkdown` document and renders it with the output
 * widgets: ATX headings (styled), paragraphs with inline emphasis
 * (`**bold**`, `*italic*`, `` `code` ``, `[text](url)` links), bullet and
 * numbered lists, fenced code blocks (in a panel), block quotes, horizontal
 * rules, and GitHub-style pipe tables.
 *
 * Not a full CommonMark implementation - it covers the common constructs.
 * Paragraph text is emitted styled and relies on the terminal's own
 * line wrapping; lists/tables wrap to `width`.
 */


/** Options for the Markdown renderer. Zero-init = terminal width and the
 *  default color scheme. */
typedef struct ScMarkdownRenderOpts {
    /** Render width in columns; `0` = terminal width. */
    int width;

    /** Heading color; zero-init = cyan (headings are also bold). */
    ScColor heading_color;

    /** Inline/code-block code color; zero-init = magenta. */
    ScColor code_color;

    /** Hyperlink text color; zero-init = blue (underlined). */
    ScColor link_color;

    /** Block-quote color; zero-init = dim. */
    ScColor quote_color;
} ScMarkdownRenderOpts;


/**
 * Renders a parsed document into a heap `ScRendered` (free with
 * `sc_rendered_free`).
 */
SPARCLI_EXPORT ScRendered *sc_capture_markdown(
    const ScMarkdown *md, ScMarkdownRenderOpts opts
);

/** Renders a parsed document to the current output stream. */
SPARCLI_EXPORT void sc_markdown_render(
    const ScMarkdown *md, ScMarkdownRenderOpts opts
);

/**
 * Convenience: parses `src` as Markdown and renders it to the current
 * output stream.
 */
SPARCLI_EXPORT void sc_markdown_render_str(
    const char *src, ScMarkdownRenderOpts opts
);

SPARCLI_END_DECLS
