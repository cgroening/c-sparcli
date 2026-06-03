//! Bordered panels and the alert presets.
//!
//!   cargo run -p sparcli --example output_panel_alert

use sparcli::*;

fn main() {
    // Minimal panel.
    panel("Hello from a panel.", PanelOpts::new().single());
    println("", Style::default());

    // Border, title, full width, centered content.
    panel(
        "Borders: single, double, rounded, thick.\n\
         Multi-line content wraps into the frame.",
        PanelOpts::new()
            .border(BorderStyle::new(BorderType::Double).color(Color::CYAN))
            .title("Settings")
            .content_align(Align::Center)
            .full_width(),
    );
    println("", Style::default());

    // Colored border + background.
    panel(
        "Colored border with a dark background.",
        PanelOpts::new()
            .border(BorderStyle::new(BorderType::Rounded).color(Color::BLUE))
            .bg(Color::rgb(20, 20, 40))
            .title("styled"),
    );
    println("", Style::default());

    // Alert presets: panel wrappers with a preset icon + color.
    alert::info("Service started on port 8080.");
    alert::debug("Cache warmed in 132 ms.");
    alert::warning("Disk usage at 85 %.");
    alert::error("Connection to database lost.\nRetrying in 5 s.");
    alert::success("All 128 tests passed.");
}
