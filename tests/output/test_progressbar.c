#include "sparcli.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>


void test_progressbar(void) {
    printf(
        "\n\n══════════════════════  PROGRESSBAR  "
        "══════════════════════\n\n"
    );

    /* ── 1. All 4 styles at 60% ── */
    {
        printf("--- 1. Alle 4 Stile (60%%) ---\n");
        ScProgressType types[] = {
            SC_PROGRESS_BLOCK,
            SC_PROGRESS_ASCII,
            SC_PROGRESS_LINE,
            SC_PROGRESS_SHADED,
        };
        const char *labels[] = { "Block ", "ASCII ", "Line  ", "Shaded" };
        for (int i = 0; i < 4; i++) {
            ScProgressBar *bar = sc_progressbar_new((ScProgressBarOpts){
                .type = types[i],
                .left_cap = "[",
                .right_cap = "]",
                .show_percent = true,
                .bar_width = 30,
                .label_width = 6,
            });
            sc_progressbar_set_label(bar, labels[i]);
            sc_progressbar_finish(bar, 0.6, 0.0);
            sc_progressbar_free(bar);
        }
    }

    /* ── 2. Block style at 0%, 25%, 50%, 75%, 100% ── */
    {
        printf("\n--- 2. Block-Stil bei 0%%, 25%%, 50%%, 75%%, 100%% ---\n");
        double ratios[] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
        for (int i = 0; i < 5; i++) {
            ScProgressBar *bar = sc_progressbar_new((ScProgressBarOpts){
                .type = SC_PROGRESS_BLOCK,
                .left_cap = "[",
                .right_cap = "]",
                .show_percent = true,
                .bar_width = 30,
            });
            sc_progressbar_finish(bar, ratios[i], 0.0);
            sc_progressbar_free(bar);
        }
    }

    /* ── 3. Threshold colors (green → yellow → red) ── */
    {
        printf(
            "\n--- 3. Schwellwert-Farben (gruen->gelb->rot) "
            "bei 30%%, 60%%, 85%% ---\n"
        );
        double ratios[] = { 0.3, 0.6, 0.85 };
        for (int i = 0; i < 3; i++) {
            ScProgressBar *bar = sc_progressbar_new((ScProgressBarOpts){
                .type = SC_PROGRESS_BLOCK,
                .left_cap = "[",
                .right_cap = "]",
                .show_percent = true,
                .bar_width = 30,
                .thresholds.enabled = true,
                .thresholds.mid = 0.5,
                .thresholds.high = 0.75,
                .thresholds.color_low = SC_ANSI_COLOR_GREEN,
                .thresholds.color_mid = SC_ANSI_COLOR_YELLOW,
                .thresholds.color_high = SC_ANSI_COLOR_RED,
            });
            sc_progressbar_finish(bar, ratios[i], 0.0);
            sc_progressbar_free(bar);
        }
    }

    /* ── 4. show_value: value/max display ── */
    {
        printf("\n--- 4. show_value (Wert/Max) mit max=250 ---\n");
        double values[] = { 0.0, 52.0, 105.0, 189.0, 250.0 };
        for (int i = 0; i < 5; i++) {
            ScProgressBar *bar = sc_progressbar_new((ScProgressBarOpts){
                .type = SC_PROGRESS_BLOCK,
                .left_cap = "[",
                .right_cap = "]",
                .show_percent = true,
                .show_value = true,
                .bar_width = 24,
                .label_width = 11,
            });
            sc_progressbar_set_label(bar, "Processing");
            sc_progressbar_finish(bar, values[i], 250.0);
            sc_progressbar_free(bar);
        }
    }

    /* ── 5. Custom caps ── */
    {
        printf("\n--- 5. Benutzerdefinierte Klammern ---\n");
        /* ❮ = U+276E = \xe2\x9d\xae, ❯ = U+276F = \xe2\x9d\xaf */
        ScProgressBar *upload_bar = sc_progressbar_new((ScProgressBarOpts){
            .type = SC_PROGRESS_SHADED,
            .left_cap = "\xe2\x9d\xae",
            .right_cap = "\xe2\x9d\xaf",
            .show_percent = true,
            .bar_width = 28,
            .label_width = 7,
        });
        sc_progressbar_set_label(upload_bar, "Upload ");
        sc_progressbar_finish(upload_bar, 0.72, 0.0);
        sc_progressbar_free(upload_bar);

        ScProgressBar *build_bar = sc_progressbar_new((ScProgressBarOpts){
            .type = SC_PROGRESS_LINE,
            .left_cap = NULL,
            .right_cap = NULL,
            .show_percent = true,
            .bar_width = 30,
            .label_width = 7,
        });
        sc_progressbar_set_label(build_bar, "Build  ");
        sc_progressbar_finish(build_bar, 0.45, 0.0);
        sc_progressbar_free(build_bar);

        ScProgressBar *deploy_bar = sc_progressbar_new((ScProgressBarOpts){
            .type = SC_PROGRESS_ASCII,
            .left_cap = "|",
            .right_cap = "|",
            .show_percent = true,
            .bar_width = 28,
            .label_width = 7,
        });
        sc_progressbar_set_label(deploy_bar, "Deploy ");
        sc_progressbar_finish(deploy_bar, 0.91, 0.0);
        sc_progressbar_free(deploy_bar);
    }

    /* ── 6. Opts strings are copied (caller buffers reused) ── */
    {
        printf("\n--- 6. Opts strings are copied ---\n");
        /* The caps come from stack buffers that are clobbered right after
           sc_progressbar_new: the bar must render its own copies. */
        char left[8];
        char right[8];
        snprintf(left, sizeof left, "<");
        snprintf(right, sizeof right, ">");
        ScProgressBar *bar = sc_progressbar_new((ScProgressBarOpts){
            .type = SC_PROGRESS_BLOCK,
            .left_cap = left,
            .right_cap = right,
            .show_percent = true,
            .bar_width = 30,
        });
        memset(left, 'X', sizeof left - 1);
        left[sizeof left - 1] = '\0';
        memset(right, 'Y', sizeof right - 1);
        right[sizeof right - 1] = '\0';
        sc_progressbar_finish(bar, 0.5, 0.0);
        sc_progressbar_free(bar);
    }
}

void test_progressbar_animated(void) {
    printf("\n--- Progressbar animated (~3s) ---\n");
    ScProgressBar *bar = sc_progressbar_new((ScProgressBarOpts){
        .type = SC_PROGRESS_BLOCK,
        .left_cap = "[",
        .right_cap = "]",
        .show_percent = true,
        .show_value = true,
        .bar_width = 40,
        .label_width = 10,
        .thresholds.enabled = true,
        .thresholds.mid = 0.33,
        .thresholds.high = 0.66,
        .thresholds.color_low = SC_ANSI_COLOR_RED,
        .thresholds.color_mid = SC_ANSI_COLOR_YELLOW,
        .thresholds.color_high = SC_ANSI_COLOR_GREEN,
    });
    sc_progressbar_set_label(bar, "Installing");
    for (int value = 0; value <= 100; value += 2) {
        sc_progressbar_draw(bar, (double)value, 100.0);
        usleep(60000);
    }
    sc_progressbar_finish(bar, 100.0, 100.0);
    sc_progressbar_free(bar);
}
