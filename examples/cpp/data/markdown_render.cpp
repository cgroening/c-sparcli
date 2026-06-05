// markdown_render.cpp - render Markdown + pretty-print an ScValue (view layer).
//
// The view layer is opt-in (serde-dependent): include <view/sparcli_view.hpp>.
//
// Run (after `make`):
//   make run-example EX=cpp/data/markdown_render

#include <view/sparcli_view.hpp>

using namespace sparcli;


int main() {
    const char* md =
        "# Report\n\n"
        "Build **passed** with `make test`.\n\n"
        "- 653 checks\n"
        "- 0 failures\n";
    view::markdown_render_str(md);

    // Pretty-print a parsed JSON value below it.
    serde::Value v = serde::json::parse(
        "{\"ok\":true,\"checks\":653,\"failures\":0}");
    view::value_render(v);
    return 0;
}
