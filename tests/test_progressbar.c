#include "sparcli.h"
#include <stdio.h>
#include <unistd.h>


void test_progressbar(void) {
    printf("\n\n══════════════════════  PROGRESSBAR  ══════════════════════\n\n");

    /* ── 1. All 4 styles at 60% ── */
    {
        printf("--- 1. Alle 4 Stile (60%%) ---\n");
        ScProgressStyle styles[] = {
            SC_PROGRESS_BLOCK, SC_PROGRESS_ASCII,
            SC_PROGRESS_LINE,  SC_PROGRESS_SHADED,
        };
        const char *names[] = { "Block ", "ASCII ", "Line  ", "Shaded" };
        for (int i = 0; i < 4; i++) {
            ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
                .style        = styles[i],
                .left_cap     = "[",
                .right_cap    = "]",
                .show_percent = 1,
                .bar_width    = 30,
                .label_width  = 6,
            });
            sc_progressbar_set_label(b, names[i]);
            sc_progressbar_finish(b, 0.6, 0.0);
            sc_progressbar_free(b);
        }
    }

    /* ── 2. Block style at 0%, 25%, 50%, 75%, 100% ── */
    {
        printf("\n--- 2. Block-Stil bei 0%%, 25%%, 50%%, 75%%, 100%% ---\n");
        double vals[] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
        for (int i = 0; i < 5; i++) {
            ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
                .style        = SC_PROGRESS_BLOCK,
                .left_cap     = "[",
                .right_cap    = "]",
                .show_percent = 1,
                .bar_width    = 30,
            });
            sc_progressbar_finish(b, vals[i], 0.0);
            sc_progressbar_free(b);
        }
    }

    /* ── 3. Threshold colors (green → yellow → red) ── */
    {
        printf("\n--- 3. Schwellwert-Farben (gruen->gelb->rot) bei 30%%, 60%%, 85%% ---\n");
        double vals[] = { 0.3, 0.6, 0.85 };
        for (int i = 0; i < 3; i++) {
            ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
                .style          = SC_PROGRESS_BLOCK,
                .left_cap       = "[",
                .right_cap      = "]",
                .show_percent   = 1,
                .bar_width      = 30,
                .thresholds.enabled = 1,
                .thresholds.mid  = 0.5,
                .thresholds.high = 0.75,
                .thresholds.color_low      = SC_ANSI_COLOR_GREEN,
                .thresholds.color_mid      = SC_ANSI_COLOR_YELLOW,
                .thresholds.color_high     = SC_ANSI_COLOR_RED,
            });
            sc_progressbar_finish(b, vals[i], 0.0);
            sc_progressbar_free(b);
        }
    }

    /* ── 4. show_value: value/max display ── */
    {
        printf("\n--- 4. show_value (Wert/Max) mit max=250 ---\n");
        double vals[] = { 0.0, 52.0, 105.0, 189.0, 250.0 };
        for (int i = 0; i < 5; i++) {
            ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
                .style        = SC_PROGRESS_BLOCK,
                .left_cap     = "[",
                .right_cap    = "]",
                .show_percent = 1,
                .show_value   = 1,
                .bar_width    = 24,
                .label_width  = 11,
            });
            sc_progressbar_set_label(b, "Processing");
            sc_progressbar_finish(b, vals[i], 250.0);
            sc_progressbar_free(b);
        }
    }

    /* ── 5. Custom caps ── */
    {
        printf("\n--- 5. Benutzerdefinierte Klammern ---\n");
        /* ❮ = U+276E = \xe2\x9d\xae, ❯ = U+276F = \xe2\x9d\xaf */
        ScProgressBar *b1 = sc_progressbar_new((ScProgressBarOpts){
            .style        = SC_PROGRESS_SHADED,
            .left_cap     = "\xe2\x9d\xae",
            .right_cap    = "\xe2\x9d\xaf",
            .show_percent = 1,
            .bar_width    = 28,
            .label_width  = 7,
        });
        sc_progressbar_set_label(b1, "Upload ");
        sc_progressbar_finish(b1, 0.72, 0.0);
        sc_progressbar_free(b1);

        ScProgressBar *b2 = sc_progressbar_new((ScProgressBarOpts){
            .style        = SC_PROGRESS_LINE,
            .left_cap     = NULL,
            .right_cap    = NULL,
            .show_percent = 1,
            .bar_width    = 30,
            .label_width  = 7,
        });
        sc_progressbar_set_label(b2, "Build  ");
        sc_progressbar_finish(b2, 0.45, 0.0);
        sc_progressbar_free(b2);

        ScProgressBar *b3 = sc_progressbar_new((ScProgressBarOpts){
            .style        = SC_PROGRESS_ASCII,
            .left_cap     = "|",
            .right_cap    = "|",
            .show_percent = 1,
            .bar_width    = 28,
            .label_width  = 7,
        });
        sc_progressbar_set_label(b3, "Deploy ");
        sc_progressbar_finish(b3, 0.91, 0.0);
        sc_progressbar_free(b3);
    }
}

void test_progressbar_animated(void) {
    printf("\n--- Progressbar animated (~3s) ---\n");
    ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
        .style          = SC_PROGRESS_BLOCK,
        .left_cap       = "[",
        .right_cap      = "]",
        .show_percent   = 1,
        .show_value     = 1,
        .bar_width      = 40,
        .label_width    = 10,
        .thresholds.enabled = 1,
        .thresholds.mid  = 0.33,
        .thresholds.high = 0.66,
        .thresholds.color_low      = SC_ANSI_COLOR_RED,
        .thresholds.color_mid      = SC_ANSI_COLOR_YELLOW,
        .thresholds.color_high     = SC_ANSI_COLOR_GREEN,
    });
    sc_progressbar_set_label(b, "Installing");
    for (int v = 0; v <= 100; v += 2) {
        sc_progressbar_draw(b, (double)v, 100.0);
        usleep(60000);
    }
    sc_progressbar_finish(b, 100.0, 100.0);
    sc_progressbar_free(b);
}
