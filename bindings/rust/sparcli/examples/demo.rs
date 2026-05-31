//! Complete showcase of the Rust wrapper: every output component and every
//! input widget.
//!
//!   cargo run -p sparcli --example demo
//!
//! The output section runs anywhere; the input section is skipped when there is
//! no interactive terminal (e.g. when piped or in CI).

use std::thread::sleep;
use std::time::Duration;

use sparcli::*;

fn heading(title: &str) {
    println("", Style::default());
    rule(
        RuleOpts::new()
            .kind(BorderType::Double)
            .title(title)
            .align(Align::Left),
    );
    println("", Style::default());
}

fn main() -> sparcli::Result<()> {
    println(
        &format!("sparcli {} — Rust wrapper demo", version_string()),
        Style::bold().fg(Color::CYAN),
    );

    output_section();

    if input_available() {
        input_section()?;
    } else {
        heading("Input widgets");
        println(
            "Skipped — run this in a real terminal to try the prompts.",
            Style::dim(),
        );
    }
    Ok(())
}

/* ── Output ──────────────────────────────────────────────────────────────── */

fn output_section() {
    // Styled text + markup.
    heading("Text & markup");
    print("attributes: ", Style::default());
    print("bold ", Style::bold());
    print("dim ", Style::dim());
    print("italic ", Style::italic());
    print("under ", Style::new().attr(Attr::UNDERLINE));
    println("rgb", Style::new().fg(Color::rgb(120, 200, 255)));
    markup::println(concat!(
        "[bold red]Error:[/] not found  [on cyan] 200 OK [/]  ",
        "[rgb(255,170,0)]warn[/]",
    ));
    let mut t = Text::new();
    t.append("multi-", Style::default())
        .append("span ", Style::bold().fg(Color::GREEN))
        .append("text", Style::italic().fg(Color::MAGENTA));
    t.print();
    println("", Style::default());

    // Panels (several borders) + alerts.
    heading("Panels & alerts");
    panel(
        "auto-fit content",
        PanelOpts::new().single().title("single"),
    );
    panel(
        "rounded, full width, centered",
        PanelOpts::new()
            .rounded()
            .title("panel")
            .content_align(Align::Center)
            .full_width(),
    );
    panel(
        "colored border + background",
        PanelOpts::new()
            .border(BorderStyle::new(BorderType::Thick).color(Color::BLUE))
            .bg(Color::rgb(20, 20, 40))
            .title("styled"),
    );
    alert::info("informational");
    alert::debug("debug detail");
    alert::warning("careful now");
    alert::error("something failed");
    alert::success("all good");

    // Badges + rule variants.
    heading("Badges & rules");
    badge(
        "NEW",
        BadgeOpts::new().style(Style::bold().bg(Color::GREEN).fg(Color::BLACK)),
    );
    print("  ", Style::default());
    badge("v1.2", BadgeOpts::new().caps("(", ")").style(Style::dim()));
    println("", Style::default());
    rule(RuleOpts::new().kind(BorderType::Single));
    rule(
        RuleOpts::new()
            .kind(BorderType::Thick)
            .title("centered")
            .width(40)
            .align(Align::Center),
    );

    // Table: header, alignment, striped, colspan, footer.
    heading("Table");
    let mut table = Table::new();
    table
        .column("Item", ColOpts::new())
        .column("Qty", ColOpts::new().align(Align::Right))
        .column("Price", ColOpts::new().align(Align::Right));
    table.row(["Apples", "12", "3.40"]);
    table.row_bg(["Bananas", "5", "1.10"], Color::rgb(30, 30, 30));
    table.row([
        Cell::new("Subtotal").colspan(2),
        Cell::skip(),
        Cell::new("4.50").align(Align::Right),
    ]);
    table.footer_row(["Total", "17", "4.50"]);
    table.print(
        TableOpts::new()
            .border(BorderType::Rounded)
            .header_row(true)
            .striped(true),
    );

    // List with nesting + tree + key/value, shown side by side via Columns.
    heading("List, tree & key/value");
    let mut list = List::new(ListOpts::new().marker(ListMarker::Number));
    let step = list.add("prepare", Style::default());
    let mut sub = step.sub(ListOpts::new().marker(ListMarker::AlphaLower));
    sub.add("gather", Style::default());
    sub.add("measure", Style::default());
    list.add("cook", Style::default());
    list.add("serve", Style::default());

    let mut tree = Tree::new(TreeOpts::new());
    let root = tree.add("project", TreeNode::root(), Style::bold());
    let src = tree.add("src", root, Style::default());
    tree.add("main.rs", src, Style::default());
    tree.add("lib.rs", src, Style::default());
    tree.add("README", root, Style::dim());

    let mut kv =
        Kv::new(KvOpts::new().key_style(Style::bold().fg(Color::CYAN)));
    kv.add("Name", "sparcli")
        .add("Lang", "C + Rust")
        .add("Version", version_string());

    let lst = capture::list(&list);
    let tre = capture::tree(&tree);
    let kvr = capture::kv(&kv);
    let mut cols = Columns::new(ColumnsOpts::new().gap(2).separator(
        BorderStyle::new(BorderType::Single).color(Color::rgb(80, 80, 100)),
    ));
    cols.add_rendered(&lst, ColItem::new())
        .add_rendered(&tre, ColItem::new())
        .add_rendered(&kvr, ColItem::new());
    cols.print();

    // Capture / compose: pad + align + vstack.
    heading("Capture / compose");
    let a = capture::panel("top", PanelOpts::new().single());
    let b = capture::panel("bottom", PanelOpts::new().single());
    if let Some(stacked) = vstack(&[&a, &b], 1) {
        stacked.align(Align::Center, 0);
    }
    pad_str(
        "padded + indented",
        PadOpts {
            left: 6,
            top: 1,
            bottom: 1,
            ..Default::default()
        },
    );

    // Progress bar (animated) + spinner (animated).
    heading("Progress & spinner");
    let mut bar = ProgressBar::new(
        ProgressBarOpts::new()
            .kind(ProgressType::Block)
            .brackets()
            .show_percent(true)
            .fill_color(Color::GREEN)
            .width(50),
    );
    bar.set_label("Installing");
    for v in 0..=100 {
        bar.draw(v as f64, 100.0);
        sleep(Duration::from_millis(8));
    }
    bar.finish(100.0, 100.0);

    let mut sp = Spinner::new(
        "Crunching",
        SpinnerOpts::new()
            .kind(SpinnerType::Dots)
            .color(Color::CYAN),
    );
    for _ in 0..24 {
        sp.tick();
        sleep(Duration::from_millis(40));
    }
    sp.finish(true, "Done");
}

/* ── Input ───────────────────────────────────────────────────────────────── */

fn input_section() -> sparcli::Result<()> {
    heading("Input widgets");

    if let Some(yes) =
        confirm("Run the input demo?", ConfirmOpts::new().default_yes(true))?
    {
        println(&format!("  confirm -> {yes}"), Style::dim());
    }

    // Text input: boxed, char-filtered, with a custom shortcut and the editor.
    let sc = Shortcuts::new().on_return(key_fn(2), 1, Some("help"));
    match text_input(
        "Name (F2 help · Ctrl-G editor)",
        TextInputOpts::new()
            .placeholder("Ada Lovelace")
            .boxed(40)
            .external_editor(true)
            .shortcuts(&sc),
    )? {
        Some(_) if sc.fired() == 1 => {
            println("  text -> F2/help requested", Style::dim())
        }
        Some(name) => println(&format!("  text -> {name:?}"), Style::dim()),
        None => println("  text -> cancelled", Style::dim()),
    }

    // Rich (partly-styled) prompt via prompt_text.
    let mut p = Text::new();
    p.append("Rename ", Style::default())
        .append("Apple", Style::italic())
        .append(" to", Style::default());
    if let Some(v) =
        text_input("", TextInputOpts::new().prompt_text(&p).initial("Apple"))?
    {
        println(&format!("  rename -> {v:?}"), Style::dim());
    }

    if let Some(pw) = password_input("Password", PasswordOpts::new().mask("•"))?
    {
        println(
            &format!("  password -> {} chars", pw.chars().count()),
            Style::dim(),
        );
    }

    if let Some(n) = number_input(
        "Quantity",
        NumberOpts::new().range(0.0, 100.0).step(5.0).initial(10.0),
    )? {
        println(&format!("  number -> {n}"), Style::dim());
    }

    if let Some(notes) = textarea(
        "Notes (Ctrl-D submit · Ctrl-G editor)",
        TextareaOpts::new().boxed(48).external_editor(true),
    )? {
        println(
            &format!("  textarea -> {} bytes", notes.len()),
            Style::dim(),
        );
    }

    // Single select.
    let mut sel = Select::new(SelectOpts::new().prompt("Pick a fruit"));
    sel.add("Apple").add("Banana").add("Cherry").add("Date");
    if let Some(i) = sel.run_one()? {
        println(&format!("  select -> index {i}"), Style::dim());
    }

    // Multi select.
    let mut multi =
        Select::new(SelectOpts::new().prompt("Pick toppings").multi(true));
    multi.add("Cheese").add("Olives").add("Basil").add("Chili");
    if let Some(idx) = multi.run()? {
        println(&format!("  multi-select -> {idx:?}"), Style::dim());
    }

    // Fuzzy finder.
    let mut fz = Fuzzy::new(FuzzyOpts::new().prompt("Find a city"));
    for c in [
        "Tokyo", "London", "Berlin", "Paris", "Oslo", "Madrid", "Lisbon",
    ] {
        fz.add(c);
    }
    if let Some(i) = fz.run()? {
        println(&format!("  fuzzy -> index {i}"), Style::dim());
    }

    // Date picker.
    if let Some(d) = datepicker(
        None,
        DatePickerOpts::new()
            .prompt("Pick a date")
            .week_start(WeekStart::Monday),
    )? {
        println(
            &format!("  date -> {:04}-{:02}-{:02}", d.year, d.month, d.day),
            Style::dim(),
        );
    }

    Ok(())
}
