#include "sparcli.h"
#include <stdio.h>
#include <unistd.h>


/**
 * Static multi-progress frame (deterministic off a TTY: the live display
 * buffers and prints only the final stack). Fixed `bar_width` keeps the
 * golden output terminal-independent.
 */
void test_multiprogress(void) {
    printf("\n");
    ScMultiProgress *mp = sc_multiprogress_begin((ScMultiProgressOpts){ 0 });

    ScProgressBarOpts bo = {
        .type = SC_PROGRESS_BLOCK, .bar_width = 20,
        .show_percent = true, .left_cap = "[", .right_cap = "]",
    };
    int a = sc_multiprogress_add(mp, "download", bo);
    int b = sc_multiprogress_add(mp, "extract",  bo);
    int c = sc_multiprogress_add(mp, "verify",   bo);

    sc_multiprogress_update(mp, a, 100, 100);
    sc_multiprogress_update(mp, b, 60, 100);
    sc_multiprogress_update(mp, c, 25, 100);

    sc_multiprogress_end(mp);
}

/** Animated demo (only meaningful on a TTY). */
void test_multiprogress_animated(void) {
    printf("\n");
    ScMultiProgress *mp = sc_multiprogress_begin((ScMultiProgressOpts){ 0 });
    ScProgressBarOpts bo = {
        .type = SC_PROGRESS_BLOCK, .bar_width = 30, .show_percent = true,
        .left_cap = "[", .right_cap = "]",
    };
    int a = sc_multiprogress_add(mp, "task A", bo);
    int b = sc_multiprogress_add(mp, "task B", bo);
    for (int i = 0; i <= 100; i++) {
        sc_multiprogress_update(mp, a, i, 100);
        sc_multiprogress_update(mp, b, i * 0.6, 100);
        usleep(20000);
    }
    sc_multiprogress_end(mp);
}
