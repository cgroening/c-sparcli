#include "sparcli.h"
#include <stdio.h>


void test_markup(void) {
    printf("\n");

    /* ── 1. Basic styles ── */
    printf("--- Markup 1. basic styles ---\n");
    sc_markup_println("[bold]bold[/]  [italic]italic[/]  [dim]dim[/]  [underline]underline[/]  [u]u-alias[/]  [bold italic underline]all three[/]");

    printf("\n");

    /* ── 2. Named foreground colors ── */
    printf("--- Markup 2. named fg colors ---\n");
    sc_markup_println("[red]red[/]  [green]green[/]  [yellow]yellow[/]  [blue]blue[/]  [magenta]magenta[/]  [cyan]cyan[/]  [white]white[/]  [black]black[/]");

    printf("\n");

    /* ── 3. Background colors ── */
    printf("--- Markup 3. background colors ---\n");
    sc_markup_println("[on blue]on blue[/]  [white on red]white on red[/]  [bold yellow on blue]bold yellow on blue[/]");

    printf("\n");

    /* ── 4. RGB colors ── */
    printf("--- Markup 4. rgb colors ---\n");
    sc_markup_println("[rgb(255,165,0)]orange warning[/]  [rgb(128,0,128)]purple[/]  [white on rgb(30,30,30)]dark bg[/]");

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
    sc_markup_println_opts("[blink]text with unknown tag[/blink]",
                           (ScMarkupOpts){ .strip_unknown = 1 });
    sc_markup_println_opts("[bold]known [blink]unknown-mid[/blink] still known[/]",
                           (ScMarkupOpts){ .strip_unknown = 1 });

    printf("\n");

    /* ── 6c. sc_markup_parse_opts side-by-side ── */
    printf("--- Markup 6c. sc_markup_parse_opts side-by-side ---\n");
    {
        const char *markup = "prefix [blink]unknown[/blink] [bold]bold[/] suffix";
        printf("  verbatim: ");
        ScText *a = sc_markup_parse(markup);
        sc_print_text(a); printf("\n");
        sc_text_free(a);
        printf("  stripped: ");
        ScText *b = sc_markup_parse_opts(markup, (ScMarkupOpts){ .strip_unknown = 1 });
        sc_print_text(b); printf("\n");
        sc_text_free(b);
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
        ScText *t = sc_text_new();
        sc_text_append(t, "Plain prefix — ", (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
        sc_markup_append(t, "[bold red]appended markup[/] — ");
        sc_markup_append(t, "[italic cyan]second append[/]");
        sc_print_text(t);
        printf("\n");
        sc_text_free(t);
    }

    printf("\n");

    /* ── 10. SC_CELL_M in a table ── */
    printf("--- Markup 10. SC_CELL_M in table ---\n");
    {
        ScTable *t = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header_row = 1,
            .header_opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(t, "Status",  (ScColOpts){ 0, 0, 14, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE });
        sc_table_add_col(t, "Message", (ScColOpts){ 0, 0, 28, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE });
        sc_table_add_row(t, (ScCell[]){ SC_CELL_M("[green]✔ OK[/]"),      SC_CELL_M("Build [bold]passed[/]")          }, 2);
        sc_table_add_row(t, (ScCell[]){ SC_CELL_M("[yellow]⚠ Warn[/]"),   SC_CELL_M("[italic]Deprecation detected[/]") }, 2);
        sc_table_add_row(t, (ScCell[]){ SC_CELL_M("[red]✖ Error[/]"),     SC_CELL_M("[bold red]Compilation failed[/]") }, 2);
        sc_table_print(t);
        sc_table_free(t);
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
            .border       = SC_BORDER_ROUNDED,
            .border_color = SC_ANSI_COLOR_CYAN,
            .title        = " Markup Demo ",
            .title_style   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
            .title_pos    = SC_TITLE_TOP,
            .title_align  = SC_ALIGN_CENTER,
            .title_pad    = 1,
            .padding = {1, 2, 1, 2},
        });
        sc_text_free(content);
    }
}
