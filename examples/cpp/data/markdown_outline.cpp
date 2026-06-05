// markdown_outline.cpp - split front matter, walk the heading outline, then
// edit the front matter from a value tree and re-serialize the document.
//
// Run (after `make`):
//   make run-example EX=cpp/data/markdown_outline

#include <serde/sparcli_serde.hpp>

#include <cstddef>
#include <optional>
#include <print>
#include <string>
#include <string_view>

using namespace sparcli::serde;

// Prints each section's title, indented by depth, depth-first.
static void print_outline(Section section, int depth) {
    for (std::size_t i = 0; i < section.child_count(); ++i) {
        Section child = section.child(i);
        std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
        std::println("{}- {}", indent, child.title());
        print_outline(child, depth + 1);
    }
}

int main() {
    Markdown md = Markdown::parse(
        "+++\ntitle = \"Guide\"\n+++\n"
        "# Install\nSteps.\n## macOS\nbrew install sparcli.\n# Usage\nRun.\n"
    );

    std::optional<std::string_view> title;
    if (auto front = md.frontmatter()) {
        title = front->get("title").as_string();
    }
    std::println("front matter title: {}", title.value_or("(none)"));
    print_outline(md.root(), 0);

    // Edit the front matter via a value tree; write() emits the new block.
    Value meta = Value::object();
    meta.set("title", Value::string("Guide"));
    meta.set("version", Value::integer(2));
    md.set_frontmatter(SC_MD_FRONTMATTER_TOML, meta);

    std::println("--- rewritten ---\n{}", md.write());
    return 0;
}
