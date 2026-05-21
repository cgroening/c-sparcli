#include "sparcli.h"
#include <stdio.h>
#include <stddef.h>


void test_columns_basic(void) {
    ScOptions plain  = { SC_STYLE_NONE, SC_COLOR_NONE,   SC_COLOR_NONE };
    ScOptions bold   = { SC_STYLE_BOLD, SC_COLOR_NONE,   SC_COLOR_NONE };
    ScOptions cyan   = { SC_STYLE_NONE, SC_COLOR_CYAN,   SC_COLOR_NONE };
    ScOptions green  = { SC_STYLE_NONE, SC_COLOR_GREEN,  SC_COLOR_NONE };
    ScOptions yellow = { SC_STYLE_NONE, SC_COLOR_YELLOW, SC_COLOR_NONE };
    ScOptions red    = { SC_STYLE_NONE, SC_COLOR_RED,    SC_COLOR_NONE };

    printf("\n");

    /* ── 1. Two string columns, no separator ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){ .gap = 3 });
        sc_columns_add_str(cl, "Left column\nLine 2\nLine 3", (ScColItem){ 0 });
        sc_columns_add_str(cl, "Right column\nLine 2",        (ScColItem){ 0 });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 2. Three columns, no separator ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){ .gap = 4 });
        sc_columns_add_str(cl, "Column A\nLine 2\nLine 3\nLine 4", (ScColItem){ 0 });
        sc_columns_add_str(cl, "Column B\nLine 2",                 (ScColItem){ 0 });
        sc_columns_add_str(cl, "Column C\nLine 2\nLine 3",         (ScColItem){ 0 });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 3. All separator styles ── */
    {
        ScBorderStyle styles[] = {
            SC_BORDER_ASCII, SC_BORDER_SINGLE, SC_BORDER_DOUBLE,
            SC_BORDER_ROUNDED, SC_BORDER_THICK,
        };
        for (size_t i = 0; i < sizeof(styles) / sizeof(*styles); i++) {
            ScColumns *cl = sc_columns_new((ScColumnsOpts){
                .gap       = 2,
                .sep_style = styles[i],
                .sep_color = SC_COLOR_NONE,
            });
            sc_columns_add_str(cl, "left side",  (ScColItem){ 0 });
            sc_columns_add_str(cl, "right side", (ScColItem){ 0 });
            sc_columns_print(cl);
            sc_columns_free(cl);
        }
    }

    printf("\n");

    /* ── 4. Separator with color ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 2,
            .sep_style = SC_BORDER_SINGLE,
            .sep_color = SC_COLOR_CYAN,
        });
        sc_columns_add_str(cl, "Left\nLine 2\nLine 3",   (ScColItem){ 0 });
        sc_columns_add_str(cl, "Middle\nLine 2\nLine 3", (ScColItem){ 0 });
        sc_columns_add_str(cl, "Right\nLine 2\nLine 3",  (ScColItem){ 0 });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 5. Vertical alignment: TOP / MIDDLE / BOTTOM ── */
    {
        const char *tall    = "Tall\nLine 2\nLine 3\nLine 4\nLine 5";
        const char *mid     = "Mid\nLine 2\nLine 3";
        const char *short_s = "Short";

        ScColumns *cl = sc_columns_new((ScColumnsOpts){ .gap = 4, .valign = SC_VALIGN_TOP });
        sc_columns_add_str(cl, tall,    (ScColItem){ .fixed_w = 10 });
        sc_columns_add_str(cl, mid,     (ScColItem){ .fixed_w = 10 });
        sc_columns_add_str(cl, short_s, (ScColItem){ .fixed_w = 10 });
        sc_columns_print(cl);
        sc_columns_free(cl);

        printf("\n");

        cl = sc_columns_new((ScColumnsOpts){ .gap = 4, .valign = SC_VALIGN_MIDDLE });
        sc_columns_add_str(cl, tall,    (ScColItem){ .fixed_w = 10 });
        sc_columns_add_str(cl, mid,     (ScColItem){ .fixed_w = 10 });
        sc_columns_add_str(cl, short_s, (ScColItem){ .fixed_w = 10 });
        sc_columns_print(cl);
        sc_columns_free(cl);

        printf("\n");

        cl = sc_columns_new((ScColumnsOpts){ .gap = 4, .valign = SC_VALIGN_BOTTOM });
        sc_columns_add_str(cl, tall,    (ScColItem){ .fixed_w = 10 });
        sc_columns_add_str(cl, mid,     (ScColItem){ .fixed_w = 10 });
        sc_columns_add_str(cl, short_s, (ScColItem){ .fixed_w = 10 });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 6. Column sizing: fixed_w / min_w / max_w ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){ .gap = 2 });
        sc_columns_add_str(cl, "fixed_w=20\nshort",
                           (ScColItem){ .fixed_w = 20 });
        sc_columns_add_str(cl, "min_w=16, auto-width",
                           (ScColItem){ .min_w = 16 });
        sc_columns_add_str(cl, "max_w=10, this text is longer",
                           (ScColItem){ .max_w = 10 });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 7. Column content alignment: LEFT / CENTER / RIGHT ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){ .gap = 2 });
        sc_columns_add_str(cl, "left",   (ScColItem){ .fixed_w = 20, .align = SC_ALIGN_LEFT   });
        sc_columns_add_str(cl, "center", (ScColItem){ .fixed_w = 20, .align = SC_ALIGN_CENTER });
        sc_columns_add_str(cl, "right",  (ScColItem){ .fixed_w = 20, .align = SC_ALIGN_RIGHT  });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 8. total_width: distribute fixed width across flex columns ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap         = 1,
            .sep_style   = SC_BORDER_SINGLE,
            .sep_color   = SC_COLOR_NONE,
            .total_width = 60,
        });
        sc_columns_add_str(cl, "Left\nsecond line",   (ScColItem){ .align = SC_ALIGN_LEFT   });
        sc_columns_add_str(cl, "Center\nsecond line", (ScColItem){ .align = SC_ALIGN_CENTER });
        sc_columns_add_str(cl, "Right\nsecond line",  (ScColItem){ .align = SC_ALIGN_RIGHT  });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 9. ScText with styles and colors ── */
    {
        ScText *t1 = sc_text_new();
        sc_text_append(t1, "Bold heading\n", bold);
        sc_text_append(t1, "normal line\n",  plain);
        sc_text_append(t1, "cyan accent",    cyan);

        ScText *t2 = sc_text_new();
        sc_text_append(t2, "Green heading\n", green);
        sc_text_append(t2, "normal line\n",   plain);
        sc_text_append(t2, "yellow accent",   yellow);

        ScText *t3 = sc_text_new();
        sc_text_append(t3, "Red heading\n", red);
        sc_text_append(t3, "normal line\n", plain);
        sc_text_append(t3, "bold accent",   bold);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 3,
            .sep_style = SC_BORDER_SINGLE,
            .sep_color = SC_COLOR_NONE,
        });
        sc_columns_add_text(cl, t1, (ScColItem){ 0 });
        sc_columns_add_text(cl, t2, (ScColItem){ 0 });
        sc_columns_add_text(cl, t3, (ScColItem){ 0 });
        sc_columns_print(cl);
        sc_columns_free(cl);

        sc_text_free(t1);
        sc_text_free(t2);
        sc_text_free(t3);
    }
}
