#include "sparcli.h"
#include <stdio.h>


static const ScTextStyle plain = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_underlined = {
    SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER,
    SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_cyan = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_yellow = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_magenta = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_red = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_green = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
};
static const ScTextStyle red = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE
};

static const ScColOpts col_left = { .halign = SC_ALIGN_LEFT };
static const ScColOpts col_right = { .halign = SC_ALIGN_RIGHT };


/** Returns a fixed-width column with the given alignment. */
static ScColOpts col_fixed(int width, ScHAlign halign) {
    return (ScColOpts){ .fixed_width = width, .halign = halign };
}

/** Returns the all-NONE-color single-border style used by most demos. */
static ScTableBorder default_border(ScBorderType style) {
    return (ScTableBorder){
        style,
        SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        0, 0, 0,
    };
}

/** Returns a centered top title with the given text and style. */
static ScTitle title_centered_top(const char *text, ScTextStyle style) {
    return (ScTitle){
        .text = text,
        .style = style,
        .halign = SC_ALIGN_CENTER,
        .pad = 1,
        .pos = SC_POSITION_TOP,
    };
}


void test_tables(void) {
    printf("\n");

    /* ── 1. Single border, header row ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Name", col_left);
        sc_table_add_column(table, "Age", col_right);
        sc_table_add_column(table, "City", col_left);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Alice"), sc_cell("30"), sc_cell("Berlin"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Bob"), sc_cell("24"), sc_cell("Hamburg"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Carol"), sc_cell("35"), sc_cell("München"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 2. Double border, header row + header col, colored separators ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Product", col_left);
        sc_table_add_column(table, "Q1", col_right);
        sc_table_add_column(table, "Q2", col_right);
        sc_table_add_column(table, "Q3", col_right);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Apples"), sc_cell("120"),
            sc_cell("95"), sc_cell("140"),
        }, 4);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Bananas"), sc_cell("80"),
            sc_cell("110"), sc_cell("70"),
        }, 4);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Cherries"), sc_cell("45"),
            sc_cell("60"), sc_cell("90"),
        }, 4);
        sc_table_print(table, (ScTableOpts){
            .border = {
                SC_BORDER_DOUBLE,
                SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
                SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_YELLOW,
                0, 0, 0,
            },
            .header.row = true,
            .header.col = true,
            .header.style = bold_cyan,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 3. Rounded border, striped rows ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Lang", col_fixed(12, SC_ALIGN_LEFT));
        sc_table_add_column(table, "Year", col_fixed(6, SC_ALIGN_RIGHT));
        sc_table_add_column(table, "Typed", col_fixed(10, SC_ALIGN_CENTER));
        sc_table_add_row(table, (ScCell[]){
            sc_cell("C"), sc_cell("1972"), sc_cell("Static"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Python"), sc_cell("1991"), sc_cell("Dynamic"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Rust"), sc_cell("2010"), sc_cell("Static"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Lua"), sc_cell("1993"), sc_cell("Dynamic"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Zig"), sc_cell("2016"), sc_cell("Static"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_ROUNDED),
            .header.row = true,
            .header.style = bold,
            .striped = true,
            .stripe_bg = sc_color_from_rgb(40, 40, 60),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 4. ScText cells with mixed styles ── */
    {
        ScText *ok = sc_text_new();
        sc_text_append(ok, "● ", bold_green);
        sc_text_append(ok, "OK", plain);

        ScText *err = sc_text_new();
        sc_text_append(err, "✗ ", red);
        sc_text_append(err, "FAIL", bold);

        ScText *label = sc_text_new();
        sc_text_append(label, "web", plain);
        sc_text_append(label, "-server", bold);

        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Service", col_left);
        sc_table_add_column(table, "Status", col_left);
        sc_table_add_column(table, "Uptime", col_right);
        sc_table_add_row(table, (ScCell[]){
            sc_cell_t(label), sc_cell_t(ok), sc_cell("99.9%"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("database"), sc_cell_t(err), sc_cell("72.1%"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("cache"), sc_cell_t(ok), sc_cell("100.0%"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold_underlined,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
        sc_text_free(ok);
        sc_text_free(err);
        sc_text_free(label);
    }

    printf("\n");

    /* ── 5. Column alignment: left / center / right ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Left", col_fixed(14, SC_ALIGN_LEFT));
        sc_table_add_column(table, "Center", col_fixed(14, SC_ALIGN_CENTER));
        sc_table_add_column(table, "Right", col_fixed(14, SC_ALIGN_RIGHT));
        sc_table_add_row(table, (ScCell[]){
            sc_cell("abc"), sc_cell("abc"), sc_cell("abc"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("longer text"),
            sc_cell("longer text"),
            sc_cell("longer text"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("x"), sc_cell("x"), sc_cell("x"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 0, 2, 0, 2 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 6. Vertical alignment: top / middle / bottom ── */
    {
        ScText *tall = sc_text_new();
        sc_text_append(tall, "Line 1\nLine 2\nLine 3\nLine 4", plain);

        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Tall Cell", col_left);
        sc_table_add_column(table, "Top", (ScColOpts){ .fixed_width = 10 });
        sc_table_add_column(table, "Middle",
            (ScColOpts){ .fixed_width = 10, .valign = SC_VALIGN_MIDDLE });
        sc_table_add_column(table, "Bottom",
            (ScColOpts){ .fixed_width = 10, .valign = SC_VALIGN_BOTTOM });
        sc_table_add_row(table, (ScCell[]){
            sc_cell_t(tall),
            sc_cell("▲ top"),
            sc_cell("● mid"),
            sc_cell("▼ bot"),
        }, 4);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 1, 1, 1, 1 },
        });
        sc_table_free(table);
        sc_text_free(tall);
    }

    printf("\n");

    /* ── 7. Fixed / min / max column widths ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Fixed=16",
            (ScColOpts){ .fixed_width = 16 });
        sc_table_add_column(table, "Min=10", (ScColOpts){ .min_width = 10 });
        sc_table_add_column(table, "Max=8",
            (ScColOpts){ .max_width = 8, .halign = SC_ALIGN_RIGHT });
        sc_table_add_column(table, "Auto", col_left);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("short"),
            sc_cell("hi"),
            sc_cell("truncated?"),
            sc_cell("auto"),
        }, 4);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("a bit longer"),
            sc_cell("pad"),
            sc_cell("clipped"),
            sc_cell("fits"),
        }, 4);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 8. No border ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Animal", col_left);
        sc_table_add_column(table, "Sound", col_left);
        sc_table_add_column(table, "Legs", col_right);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Dog"), sc_cell("Woof"), sc_cell("4"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Bird"), sc_cell("Tweet"), sc_cell("2"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Snake"), sc_cell("Hiss"), sc_cell("0"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_NONE),
            .header.row = true,
            .header.style = bold_underlined,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 9. ASCII border ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Key", col_left);
        sc_table_add_column(table, "Value", col_left);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("host"), sc_cell("localhost"),
        }, 2);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("port"), sc_cell("8080"),
        }, 2);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("timeout"), sc_cell("30s"),
        }, 2);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_ASCII),
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 10. Thick border, golden color ── */
    {
        ScColor gold = sc_color_from_rgb(255, 180, 0);
        ScTextStyle bold_gold = { SC_TEXT_ATTR_BOLD, gold, SC_ANSI_COLOR_NONE };

        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "OS", col_left);
        sc_table_add_column(table, "Kernel",
            (ScColOpts){ .halign = SC_ALIGN_CENTER });
        sc_table_add_column(table, "Shell", col_left);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Linux"), sc_cell("6.8"), sc_cell("bash"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("macOS"), sc_cell("XNU"), sc_cell("zsh"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Windows"), sc_cell("NT 10"), sc_cell("pwsh"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = {
                SC_BORDER_THICK,
                gold, SC_ANSI_COLOR_NONE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                0, 0, 0,
            },
            .header.row = true,
            .header.style = bold_gold,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 11. Word wrap ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Component", col_fixed(14, SC_ALIGN_LEFT));
        sc_table_add_column(table, "Description",
            (ScColOpts){ .fixed_width = 24, .word_wrap = 1 });
        sc_table_add_column(table, "Status", (ScColOpts){
            .fixed_width = 10, .halign = SC_ALIGN_CENTER,
            .valign = SC_VALIGN_MIDDLE });
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Auth Service"),
            sc_cell(
                "Handles OAuth2 token validation and refresh "
                "for all API clients"
            ),
            sc_cell("OK"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Data Pipeline"),
            sc_cell(
                "Ingests streaming events from Kafka and writes "
                "to the data warehouse"
            ),
            sc_cell("WARN"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Scheduler"),
            sc_cell(
                "Runs periodic jobs using a distributed "
                "cron-like mechanism"
            ),
            sc_cell("OK"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = {
                SC_BORDER_DOUBLE,
                SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                0, 0, 0,
            },
            .header.row = true,
            .header.style = bold,
            .title = title_centered_top(" Word Wrap ", bold_cyan),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 18. Footer row ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Item", col_left);
        sc_table_add_column(table, "Qty", col_fixed(6, SC_ALIGN_RIGHT));
        sc_table_add_column(table, "Price", col_fixed(9, SC_ALIGN_RIGHT));
        sc_table_add_column(table, "Total", col_fixed(10, SC_ALIGN_RIGHT));
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Widget A"), sc_cell("3"),
            sc_cell("$ 12.00"), sc_cell("$  36.00"),
        }, 4);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Gadget B"), sc_cell("1"),
            sc_cell("$ 49.99"), sc_cell("$  49.99"),
        }, 4);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Doohickey"), sc_cell("5"),
            sc_cell("$  4.50"), sc_cell("$  22.50"),
        }, 4);
        sc_table_add_footer_row(table, (ScCell[]){
            sc_cell("TOTAL"), sc_cell("9"),
            sc_cell(""), sc_cell("$ 108.49"),
        }, 4);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .footer.row_bg = sc_color_from_rgb(30, 40, 30),
            .footer.style = bold,
            .title = title_centered_top(" Footer Row (Totals) ", bold_green),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 19. Per-row background ── */
    {
        ScColor fail_bg = sc_color_from_rgb(60, 20, 20);
        ScColor pass_bg = sc_color_from_rgb(20, 50, 20);

        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Build", col_left);
        sc_table_add_column(table, "Branch", col_left);
        sc_table_add_column(table, "Result",
            (ScColOpts){ .halign = SC_ALIGN_CENTER });
        sc_table_add_column(table, "Time", col_right);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("#101"), sc_cell("main"),
            sc_cell("PASS"), sc_cell("1m 23s"),
        }, 4);
        sc_table_add_row_bg(table, (ScCell[]){
            sc_cell("#102"), sc_cell("feature/x"),
            sc_cell("FAIL"), sc_cell("0m 47s"),
        }, 4, fail_bg);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("#103"), sc_cell("main"),
            sc_cell("PASS"), sc_cell("1m 31s"),
        }, 4);
        sc_table_add_row_bg(table, (ScCell[]){
            sc_cell("#104"), sc_cell("hotfix/y"),
            sc_cell("PASS"), sc_cell("2m 05s"),
        }, 4, pass_bg);
        sc_table_add_row_bg(table, (ScCell[]){
            sc_cell("#105"), sc_cell("release/2"),
            sc_cell("FAIL"), sc_cell("3m 12s"),
        }, 4, fail_bg);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .title = title_centered_top(
                " Per-Row Background ", bold_yellow
            ),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 20. Per-column background ── */
    {
        ScColor pro_bg = sc_color_from_rgb(20, 45, 90);
        ScColor enterprise_bg = sc_color_from_rgb(60, 40, 0);
        ScColor header_bg = sc_color_from_rgb(30, 30, 50);
        ScTextStyle header_style = {
            SC_TEXT_ATTR_BOLD,
            sc_color_from_rgb(200, 200, 255),
            SC_ANSI_COLOR_NONE,
        };

        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Feature", col_fixed(18, SC_ALIGN_LEFT));
        sc_table_add_column(table, "Free", col_fixed(10, SC_ALIGN_CENTER));
        sc_table_add_column(table, "Pro", (ScColOpts){
            .fixed_width = 10, .halign = SC_ALIGN_CENTER, .bg = pro_bg });
        sc_table_add_column(table, "Enterprise", (ScColOpts){
            .fixed_width = 12, .halign = SC_ALIGN_CENTER,
            .bg = enterprise_bg });
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Storage"), sc_cell("5 GB"),
            sc_cell("50 GB"), sc_cell("Unlimited"),
        }, 4);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("API calls/mo"), sc_cell("1 000"),
            sc_cell("50 000"), sc_cell("Unlimited"),
        }, 4);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Team members"), sc_cell("1"),
            sc_cell("25"), sc_cell("Unlimited"),
        }, 4);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Support"), sc_cell("–"),
            sc_cell("Email"), sc_cell("24/7"),
        }, 4);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Custom domain"), sc_cell("✗"),
            sc_cell("✓"), sc_cell("✓"),
        }, 4);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.row_bg = header_bg,
            .header.style = header_style,
            .title = title_centered_top(
                " Per-Column Background ", bold_cyan
            ),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 21. Total width ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Country", col_left);
        sc_table_add_column(table, "Capital", col_left);
        sc_table_add_column(table, "Population", col_right);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Germany"), sc_cell("Berlin"), sc_cell("83M"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("France"), sc_cell("Paris"), sc_cell("68M"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Netherlands"), sc_cell("Amsterdam"), sc_cell("18M"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .total_width = 60,
            .title = title_centered_top(
                " Total Width = 60 cols ", bold_magenta
            ),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 22. Colspan ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Name", col_fixed(12, SC_ALIGN_LEFT));
        sc_table_add_column(table, "Q1", col_fixed(6, SC_ALIGN_RIGHT));
        sc_table_add_column(table, "Q2", col_fixed(6, SC_ALIGN_RIGHT));
        sc_table_add_column(table, "Q3", col_fixed(6, SC_ALIGN_RIGHT));
        sc_table_add_column(table, "Q4", col_fixed(6, SC_ALIGN_RIGHT));
        sc_table_add_row(table, (ScCell[]){
            sc_cell(""),
            sc_cell_cs("H1 (Q1+Q2)", 2), sc_cell_skip(),
            sc_cell_cs("H2 (Q3+Q4)", 2), sc_cell_skip(),
        }, 5);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Alice"),
            sc_cell("120"), sc_cell("95"),
            sc_cell("140"), sc_cell("110"),
        }, 5);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Bob"),
            sc_cell("80"), sc_cell("110"),
            sc_cell("70"), sc_cell("95"),
        }, 5);
        sc_table_add_row(table, (ScCell[]){
            sc_cell_csa("* values in units sold", 5, SC_ALIGN_CENTER),
            sc_cell_skip(), sc_cell_skip(),
            sc_cell_skip(), sc_cell_skip(),
        }, 5);
        sc_table_print(table, (ScTableOpts){
            .border = {
                SC_BORDER_SINGLE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE,
                0, 0, 0,
            },
            .header.row = true,
            .header.style = bold,
            .title = title_centered_top(" Colspan ", bold_yellow),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 23. Rowspan ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Category",
            (ScColOpts){ .fixed_width = 12, .valign = SC_VALIGN_MIDDLE });
        sc_table_add_column(table, "Item", col_left);
        sc_table_add_column(table, "Price", col_fixed(9, SC_ALIGN_RIGHT));
        sc_table_add_row(table, (ScCell[]){
            sc_cell_rs("Fruits", 3),
            sc_cell("Apple"), sc_cell("$  0.50"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_row_skip(),
            sc_cell("Banana"), sc_cell("$  0.30"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_row_skip(),
            sc_cell("Cherry"), sc_cell("$  2.00"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell_rs("Veggies", 2),
            sc_cell("Carrot"), sc_cell("$  0.80"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_row_skip(),
            sc_cell("Pepper"), sc_cell("$  1.20"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .title = title_centered_top(" Rowspan ", bold_cyan),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 24. RTL column order ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Col A", col_fixed(10, SC_ALIGN_LEFT));
        sc_table_add_column(table, "Col B", col_fixed(10, SC_ALIGN_CENTER));
        sc_table_add_column(table, "Col C", col_fixed(10, SC_ALIGN_RIGHT));
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Alpha"), sc_cell("Beta"), sc_cell("Gamma"),
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("1"), sc_cell("2"), sc_cell("3"),
        }, 3);
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .right_to_left = true,
            .title = title_centered_top(" RTL Column Order ", bold_cyan),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }

    printf("\n");

    /* ── 25. Max rows ── */
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "#", col_fixed(4, SC_ALIGN_RIGHT));
        sc_table_add_column(table, "Name", col_fixed(12, SC_ALIGN_LEFT));
        sc_table_add_column(table, "Score", col_fixed(7, SC_ALIGN_RIGHT));

        /*
         * Strings must outlive sc_table_print, so the buffers cannot live in
         * the loop scope – use one buffer slot per row.
         */
        char row_index[10][4];
        char row_name[10][12];
        char row_score[10][8];
        for (int i = 1; i <= 10; i++) {
            snprintf(row_index[i - 1], sizeof(row_index[i - 1]), "%d", i);
            snprintf(
                row_name[i - 1], sizeof(row_name[i - 1]), "Player %d", i
            );
            snprintf(
                row_score[i - 1], sizeof(row_score[i - 1]),
                "%d", 10000 - i * 850
            );
            sc_table_add_row(table, (ScCell[]){
                sc_cell(row_index[i - 1]),
                sc_cell(row_name[i - 1]),
                sc_cell(row_score[i - 1]),
            }, 3);
        }
        sc_table_print(table, (ScTableOpts){
            .border = default_border(SC_BORDER_SINGLE),
            .header.row = true,
            .header.style = bold,
            .max_rows = 4,
            .title = title_centered_top(
                " Max Rows = 4 (10 total) ", bold_red
            ),
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_table_free(table);
    }
}
