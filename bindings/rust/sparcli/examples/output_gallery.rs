//! A static gallery of the output widgets. `cargo run --example output_gallery`.

use sparcli::{
    alert, capture, markup, panel, println as scprintln, rule, vstack, Align, BorderType, Color,
    ColItem, Columns, ColumnsOpts, Kv, KvOpts, List, ListMarker, ListOpts, PanelOpts, RuleOpts,
    Style, Table, TableOpts, Text,
};

fn main() {
    rule(RuleOpts::new().kind(BorderType::Double).title("sparcli (Rust)"));
    scprintln("", Style::default());

    panel(
        "Rust talking to the C core.",
        PanelOpts::new().rounded().title("panel").full_width(),
    );

    // Rich text + markup.
    let mut t = Text::new();
    t.append("status: ", Style::default())
        .append("OK", Style::bold().fg(Color::GREEN));
    t.print();
    scprintln("", Style::default());
    markup::println("[bold magenta]markup[/] works too — [italic]nice[/].");
    scprintln("", Style::default());

    // Table.
    let mut table = Table::new();
    table
        .column("Lang", Default::default())
        .column("Typing", Default::default());
    table.row(["C", "static"]).row(["Rust", "static, safe"]);
    table.print(
        TableOpts::new()
            .border(BorderType::Rounded)
            .header_row(true)
            .striped(true),
    );
    scprintln("", Style::default());

    // List.
    let mut list = List::new(ListOpts::new().marker(ListMarker::Number));
    list.add("first", Style::default());
    let item = list.add("second", Style::default());
    let mut sub = item.sub(ListOpts::new().marker(ListMarker::AlphaLower));
    sub.add("nested a", Style::default());
    sub.add("nested b", Style::default());
    list.print();
    scprintln("", Style::default());

    // Key/value.
    let mut kv = Kv::new(KvOpts::new().key_style(Style::bold()));
    kv.add("Version", sparcli::version_string()).add("Bindings", "Rust");
    kv.print();
    scprintln("", Style::default());

    // Columns: two captured panels side by side.
    let left = capture::panel("left", PanelOpts::new().single());
    let right = capture::panel("right", PanelOpts::new().single());
    let _ = vstack(&[&left, &right], 0); // demo vstack too
    let mut cols = Columns::new(ColumnsOpts::new().gap(4));
    cols.add_rendered(&left, ColItem::new())
        .add_rendered(&right, ColItem::new().align(Align::Center));
    cols.print();
    scprintln("", Style::default());

    // Alerts.
    alert::success("Everything compiled and linked.");
    alert::warning("This is just a demo.");
}
