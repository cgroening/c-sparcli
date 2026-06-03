/*
 * panel_alert.c - bordered panels (titles, padding, alignment) and the
 * alert presets built on top of them.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/panel_alert
 */

#include <sparcli.h>

#include <stdio.h>


int main(void) {
    // Minimal panel: only a border type; every other option keeps its
    // zero-init default.
    sc_panel_str("Hello from a panel.", (ScPanelOpts){
        .border = { .type = SC_BORDER_SINGLE },
    });
    printf("\n");

    // Border style, title, inner padding and a fixed width.
    sc_panel_str(
        "Borders: NONE, ASCII, SINGLE, DOUBLE, ROUNDED, THICK.\n"
        "Multi-line content wraps into the frame.",
        (ScPanelOpts){
            .border  = { .type = SC_BORDER_DOUBLE, .color = SC_ANSI_COLOR_CYAN },
            .title   = {
                .text   = "Settings",
                .style  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                            SC_ANSI_COLOR_NONE },
                .halign = SC_ALIGN_LEFT,
                .pad    = 1,
            },
            .padding = { .top = 1, .right = 2, .bottom = 1, .left = 2 },
            .width   = 56,
        }
    );
    printf("\n");

    // Title + subtitle on opposite edges, centered content, rich text body.
    ScText *body = sc_markup_parse("Centered [bold green]rich text[/] body.");
    sc_panel_text(body, (ScPanelOpts){
        .border        = { .type = SC_BORDER_ROUNDED },
        .title         = { .text = "Report", .halign = SC_ALIGN_CENTER },
        .subtitle      = {
            .text   = "2026-06-03",
            .style  = { .attr = SC_TEXT_ATTR_DIM },
            .halign = SC_ALIGN_RIGHT,
            .pos    = SC_POSITION_BOTTOM,
        },
        .width         = 56,
        .content_align = SC_ALIGN_CENTER,
    });
    sc_text_free(body);
    printf("\n");

    // full_width stretches the frame to the terminal width.
    sc_panel_str("This panel spans the whole terminal.", (ScPanelOpts){
        .border     = { .type = SC_BORDER_SINGLE },
        .full_width = true,
    });
    printf("\n");

    // Alert presets: thin wrappers over the panel with icon + color.
    sc_alert_info("Service started on port 8080.");
    sc_alert_debug("Cache warmed in 132 ms.");
    sc_alert_warning("Disk usage at 85 %.");
    sc_alert_error("Connection to database lost.\nRetrying in 5 s.");
    sc_alert_success("All 128 tests passed.");

    return 0;
}
