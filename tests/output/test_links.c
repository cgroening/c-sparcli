#include "sparcli.h"
#include <stdio.h>


static const ScTextStyle plain = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle cyan_under = {
    SC_TEXT_ATTR_UNDER, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
};


void test_links(void) {
    printf("\n");

    /* ── 1. sc_text_append_link API ── */
    printf("--- Links 1. sc_text_append_link ---\n");
    {
        ScText *text = sc_text_new();
        sc_text_append(text, "See the ", plain);
        sc_text_append_link(
            text, "sparcli repository", "https://example.com/sparcli",
            cyan_under
        );
        sc_text_append(text, " for details.", plain);
        sc_print_text(text);
        printf("\n");
        sc_text_free(text);
    }

    printf("\n");

    /* ── 2. NULL / empty URL degrades to a plain span ── */
    printf("--- Links 2. NULL / empty URL ---\n");
    {
        ScText *text = sc_text_new();
        sc_text_append_link(text, "no url at all", NULL, plain);
        sc_text_append(text, " | ", plain);
        sc_text_append_link(text, "empty url", "", plain);
        sc_print_text(text);
        printf("\n");
        sc_text_free(text);
    }

    printf("\n");

    /* ── 3. URL scrubbing: control bytes and ESC removed ── */
    printf("--- Links 3. URL scrubbing ---\n");
    {
        ScText *text = sc_text_new();
        sc_text_append_link(
            text, "scrubbed", "https://example.com/\033[31m\a\npath", plain
        );
        sc_print_text(text);
        printf("\n");
        sc_text_free(text);
    }

    printf("\n");

    /* ── 4. Markup [link=URL]…[/link] ── */
    printf("--- Links 4. markup [link=URL] ---\n");
    sc_markup_println(
        "Open [link=https://example.com/docs]the documentation[/link] now"
    );
    sc_markup_println(
        "Closed with the generic tag: [link=https://example.com]click here[/]"
    );
    sc_markup_println(
        "[bold]Styled outside: [link=https://example.com]bold link[/link][/]"
    );

    printf("\n");

    /* ── 5. Markup edge cases ── */
    printf("--- Links 5. markup edge cases ---\n");
    sc_markup_println("Empty URL stays verbatim: [link=]text[/link]");
    sc_markup_println("Unterminated link: [link=https://example.com]tail");
    sc_markup_println(
        "Body is literal: [link=https://example.com]a [bold]b[/bold][/link]"
    );

    printf("\n");

    /* ── 6. Link inside a panel (alignment must be unaffected) ── */
    printf("--- Links 6. link in panel ---\n");
    {
        ScText *content = sc_markup_parse(
            "Read the [link=https://example.com/manual]manual[/link]\n"
            "or the plain line below it for comparison."
        );
        sc_panel_text(content, (ScPanelOpts){
            .border = { .type = SC_BORDER_SINGLE },
            .title = {
                .text = " Links ",
                .style = bold,
                .halign = SC_ALIGN_LEFT,
                .pad = 1,
                .pos = SC_POSITION_TOP,
            },
            .padding = { 0, 1, 0, 1 },
        });
        sc_text_free(content);
    }

    printf("\n");

    /* ── 7. Link inside a table cell (alignment must be unaffected) ── */
    printf("--- Links 7. link in table ---\n");
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Project", (ScColOpts){ 0 });
        sc_table_add_column(table, "Homepage", (ScColOpts){ 0 });
        sc_table_add_row(table, (ScCell[]){
            sc_cell("sparcli"),
            sc_cell_m("[link=https://example.com/sparcli]example.com[/link]"),
        }, 2);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("plain row"),
            sc_cell("no link"),
        }, 2);
        sc_table_print(table, (ScTableOpts){
            .border = { .type = SC_BORDER_SINGLE },
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }
}
