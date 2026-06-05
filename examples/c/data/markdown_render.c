/*
 * markdown_render.c - render Markdown to the terminal through the widget stack
 * (view layer: headings, lists, code blocks, quotes, tables, inline emphasis).
 *
 * The view layer is opt-in: include <view/sparcli_view.h> explicitly.
 *
 * Run (after `make`):
 *   make run-example EX=c/data/markdown_render
 */

#include <sparcli.h>
#include <view/sparcli_view.h>


int main(void) {
    const char *md =
        "# sparcli\n\n"
        "A C library for **styled terminal output** and *interactive* input.\n\n"
        "## Features\n\n"
        "- Panels, tables, lists, trees\n"
        "- Input widgets with `sc_prompt_run`\n"
        "- [Documentation](https://example.com/sparcli)\n\n"
        "## Status\n\n"
        "> All tests green.\n\n"
        "| Component | State |\n"
        "|-----------|------:|\n"
        "| output    | done  |\n"
        "| input     | done  |\n\n"
        "```\nmake test   # 653 checks\n```\n";

    sc_markdown_render_str(md, (ScMarkdownRenderOpts){ 0 });
    return 0;
}
