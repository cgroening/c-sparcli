// panel_alert.cpp - bordered panels and the alert presets (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/panel_alert

#include <sparcli.hpp>

using namespace sparcli;


int main() {
    // Minimal panel: only the border type, every other field zero-init.
    panel("Hello from a panel.", { .border = { .type = SC_BORDER_SINGLE } });
    println("");

    // Border, title, padding and a fixed width.
    panel("Borders: NONE, ASCII, SINGLE, DOUBLE, ROUNDED, THICK.\n"
          "Multi-line content wraps into the frame.",
          { .border  = { .type = SC_BORDER_DOUBLE, .color = cyan() },
            .title   = { .text   = "Settings",
                         .style  = style(SC_TEXT_ATTR_BOLD, cyan()),
                         .halign = SC_ALIGN_LEFT, .pad = 1 },
            .width   = 56, .padding = { 1, 2, 1, 2 } });
    println("");

    // Title + subtitle on opposite edges, centered rich-text content.
    Text body = markup::parse("Centered [bold green]rich text[/] body.");
    panel(body, { .border        = { .type = SC_BORDER_ROUNDED },
                  .title         = { .text = "Report",
                                     .halign = SC_ALIGN_CENTER },
                  .subtitle      = { .text = "2026-06-03",
                                     .style = style(SC_TEXT_ATTR_DIM),
                                     .halign = SC_ALIGN_RIGHT,
                                     .pos = SC_POSITION_BOTTOM },
                  .width         = 56,
                  .content_align = SC_ALIGN_CENTER });
    println("");

    panel("This panel spans the whole terminal.",
          { .border = { .type = SC_BORDER_SINGLE }, .full_width = true });
    println("");

    // Alert presets: panel wrappers with a preset icon + color.
    alert::info("Service started on port 8080.");
    alert::debug("Cache warmed in 132 ms.");
    alert::warning("Disk usage at 85 %.");
    alert::error("Connection to database lost.\nRetrying in 5 s.");
    alert::success("All 128 tests passed.");

    return 0;
}
