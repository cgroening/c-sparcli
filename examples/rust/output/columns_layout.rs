//! Side-by-side columns, capture/vstack composition, pad and align.
//!
//!   cargo run -p sparcli --example output_columns_layout

use sparcli::*;

fn main() {
    print_basic_columns();
    println("", Style::default());
    print_composed_layout();
    println("", Style::default());
    print_pad_and_align();
}

/// Columns with a separator.
fn print_basic_columns() {
    let mut cols = Columns::new(
        ColumnsOpts::new()
            .gap(2)
            .separator(BorderStyle::new(BorderType::Single).color(Color::CYAN)),
    );
    cols.add_str("Left column\nwith two lines.", ColItem::new())
        .add_str("Right column\naligned left.", ColItem::new());
    cols.print();
}

/// Capture widgets, stack them vertically, place the stack in a column.
fn print_composed_layout() {
    let mut list = List::new(ListOpts::new().marker(ListMarker::Number));
    list.add("capture widgets", Style::default());
    list.add("stack them with vstack", Style::default());
    list.add("drop the stack into a column", Style::default());

    let r_list = capture::list(&list);
    let r_rule = capture::rule(RuleOpts::new().kind(BorderType::Single).width(32).title("composed"));
    let stack = vstack(&[&r_list, &r_rule], 1).expect("two parts");

    let panel = capture::panel(
        "Columns copy captured\ncontent, so sources\ncan be freed right away.",
        PanelOpts::new().single(),
    );
    let mut cols = Columns::new(ColumnsOpts::new().gap(4));
    cols.add_rendered(&stack, ColItem::new())
        .add_rendered(&panel, ColItem::new());
    cols.print();
}

/// Pad and align a captured block, then the one-step string helpers.
fn print_pad_and_align() {
    let block = capture::panel("padded + centered", PanelOpts::new().thick());
    block.pad(PadOpts { top: 1, left: 4, ..Default::default() });
    block.align(Align::Center, 0);

    pad_str("indented by eight columns", PadOpts { left: 8, ..Default::default() });
    align_str("centered heading", Align::Center, 0);
}
