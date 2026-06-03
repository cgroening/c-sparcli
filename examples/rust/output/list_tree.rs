//! Nested lists and tree views.
//!
//!   cargo run -p sparcli --example output_list_tree

use sparcli::*;

fn main() {
    print_lists();
    println("", Style::default());
    print_tree();
}

/// Markers, nesting and a styled marker.
fn print_lists() {
    let mut steps = List::new(ListOpts::new().marker(ListMarker::Number));
    steps.add("Check out the repository", Style::default());
    let build = steps.add("Build the project", Style::default());
    steps.add("Run the test suite", Style::default());

    // A sub-list is owned by the parent item.
    let mut sub = build.sub(ListOpts::new().marker(ListMarker::AlphaLower).indent(2));
    sub.add("make", Style::default());
    sub.add("make qa", Style::default());
    steps.print();
    println("", Style::default());

    let mut notes = List::new(ListOpts::new().marker(ListMarker::Bullet).bullet("→"));
    notes.add(
        "Long items are word-wrapped and continuation lines align under \
         the first word of the item.",
        Style::default(),
    );
    notes.print();
}

/// A file-system-like tree; `add` returns a node usable as a parent.
fn print_tree() {
    let mut tree = Tree::new(TreeOpts::new());
    let root = tree.add("sparcli/", TreeNode::root(), Style::bold().fg(Color::BLUE));
    let src = tree.add("src/", root, Style::bold().fg(Color::BLUE));
    tree.add("core/", src, Style::default());
    tree.add("output/", src, Style::default());
    let docs = tree.add("docs/", root, Style::bold().fg(Color::BLUE));
    tree.add("examples.md", docs, Style::new().fg(Color::GREEN));
    tree.add("Makefile", root, Style::default());
    tree.print();
}
