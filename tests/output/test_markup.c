#include "sparcli.h"
#include <stdio.h>


static const ScTextStyle plain = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_cyan = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
};


void test_markup(void) {
    printf("\n");

    /* ── 1. Basic styles ── */
    printf("--- Markup 1. basic styles ---\n");
    sc_markup_println(
        "[bold]bold[/]  [italic]italic[/]  [dim]dim[/]  "
        "[underline]underline[/]  [u]u-alias[/]  "
        "[bold italic underline]all three[/]"
    );

    printf("\n");

    /* ── 2. Named foreground colors ── */
    printf("--- Markup 2. named fg colors ---\n");
    sc_markup_println(
        "[red]red[/]  [green]green[/]  [yellow]yellow[/]  [blue]blue[/]  "
        "[magenta]magenta[/]  [cyan]cyan[/]  [white]white[/]  [black]black[/]"
    );

    printf("\n");

    /* ── 3. Background colors ── */
    printf("--- Markup 3. background colors ---\n");
    sc_markup_println(
        "[on blue]on blue[/]  [white on red]white on red[/]  "
        "[bold yellow on blue]bold yellow on blue[/]"
    );

    printf("\n");

    /* ── 4. RGB colors ── */
    printf("--- Markup 4. rgb colors ---\n");
    sc_markup_println(
        "[rgb(255,165,0)]orange warning[/]  [rgb(128,0,128)]purple[/]  "
        "[white on rgb(30,30,30)]dark bg[/]"
    );

    printf("\n");

    /* ── 5. Nesting / stack ── */
    printf("--- Markup 5. nesting / stack ---\n");
    sc_markup_println("[bold][red]bold+red[/] still bold[/] plain");

    printf("\n");

    /* ── 6a. Unknown tags: verbatim (default) ── */
    printf("--- Markup 6a. unknown tags → verbatim (default) ---\n");
    sc_markup_println("[blink]text with unknown tag[/blink]");
    sc_markup_println("[bold]known [blink]unknown-mid[/blink] still known[/]");

    printf("\n");

    /* ── 6b. Unknown tags: stripped ── */
    printf("--- Markup 6b. unknown tags → stripped (strip_unknown=1) ---\n");
    sc_markup_println_opts(
        "[blink]text with unknown tag[/blink]",
        (ScMarkupOpts){ .strip_unknown = true }
    );
    sc_markup_println_opts(
        "[bold]known [blink]unknown-mid[/blink] still known[/]",
        (ScMarkupOpts){ .strip_unknown = true }
    );

    printf("\n");

    /* ── 6c. sc_markup_parse_opts side-by-side ── */
    printf("--- Markup 6c. sc_markup_parse_opts side-by-side ---\n");
    {
        const char *markup = "prefix [blink]unknown[/blink] [bold]bold[/] suffix";

        printf("  verbatim: ");
        ScText *verbatim = sc_markup_parse(markup);
        sc_print_text(verbatim);
        printf("\n");
        sc_text_free(verbatim);

        printf("  stripped: ");
        ScText *stripped = sc_markup_parse_opts(
            markup, (ScMarkupOpts){ .strip_unknown = true }
        );
        sc_print_text(stripped);
        printf("\n");
        sc_text_free(stripped);
    }

    printf("\n");

    /* ── 7. [[ → literal [ ── */
    printf("--- Markup 7. [[ escape ---\n");
    sc_markup_println("[[bold]] is a literal bracket pair");
    sc_markup_println("[bold]styled [[with brackets]][/]");

    printf("\n");

    /* ── 8. sc_markup_print / sc_markup_println ── */
    printf("--- Markup 8. sc_markup_print + println ---\n");
    sc_markup_print("[green]print (no newline)[/green] ");
    sc_markup_print("[yellow]still same line[/]");
    printf("\n");
    sc_markup_println("[cyan]println (adds newline)[/]");

    printf("\n");

    /* ── 9. sc_markup_append into existing ScText ── */
    printf("--- Markup 9. sc_markup_append ---\n");
    {
        ScText *text = sc_text_new();
        sc_text_append(text, "Plain prefix — ", plain);
        sc_markup_append(text, "[bold red]appended markup[/] — ");
        sc_markup_append(text, "[italic cyan]second append[/]");
        sc_print_text(text);
        printf("\n");
        sc_text_free(text);
    }

    printf("\n");

    /* ── 10. sc_cell_m in a table ── */
    printf("--- Markup 10. sc_cell_m in table ---\n");
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Status", (ScColOpts){ .fixed_width = 14 });
        sc_table_add_column(table, "Message", (ScColOpts){ .fixed_width = 28 });
        sc_table_add_row(table, (ScCell[]){
            sc_cell_m("[green]✔ OK[/]"),
            sc_cell_m("Build [bold]passed[/]"),
        }, 2);
        sc_table_add_row(table, (ScCell[]){
            sc_cell_m("[yellow]⚠ Warn[/]"),
            sc_cell_m("[italic]Deprecation detected[/]"),
        }, 2);
        sc_table_add_row(table, (ScCell[]){
            sc_cell_m("[red]✖ Error[/]"),
            sc_cell_m("[bold red]Compilation failed[/]"),
        }, 2);
        sc_table_print(table, (ScTableOpts){
            .border = {
                SC_BORDER_SINGLE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                0, 0, 0
            },
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 11. sc_markup_parse + sc_panel_text ── */
    printf("--- Markup 11. sc_markup_parse + sc_panel_text ---\n");
    {
        ScText *content = sc_markup_parse(
            "[bold]sparcli[/] supports [italic]Rich-compatible[/] markup.\n"
            "Colors: [red]red[/], [green]green[/], [blue]blue[/].\n"
            "Combine: [bold yellow on blue] bold yellow on blue [/]"
        );
        sc_panel_text(content, (ScPanelOpts){
            .border = {
                .type = SC_BORDER_ROUNDED,
                .color = SC_ANSI_COLOR_CYAN,
            },
            .title = {
                .text = " Markup Demo ",
                .style = bold_cyan,
                .halign = SC_ALIGN_CENTER,
                .pad = 1,
                .pos = SC_POSITION_TOP,
            },
            .padding = { 1, 2, 1, 2 },
        });
        sc_text_free(content);
    }
}
