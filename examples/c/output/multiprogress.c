/*
 * multiprogress.c - several progress bars updated together in place.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/multiprogress
 *
 * On a terminal the three bars animate in place; piped/redirected, only the
 * final stack is printed (the live display buffers off a TTY).
 */

#include <sparcli.h>

#include <unistd.h>


int main(void) {
    ScMultiProgress *mp = sc_multiprogress_begin((ScMultiProgressOpts){ 0 });

    ScProgressBarOpts bo = {
        .type = SC_PROGRESS_BLOCK, .bar_width = 24, .show_percent = true,
        .left_cap = "[", .right_cap = "]",
    };
    int download = sc_multiprogress_add(mp, "download", bo);
    int extract  = sc_multiprogress_add(mp, "extract",  bo);
    int verify   = sc_multiprogress_add(mp, "verify",   bo);

    for (int i = 0; i <= 100; i++) {
        sc_multiprogress_update(mp, download, i, 100);
        sc_multiprogress_update(mp, extract,  i * 0.7, 100);
        sc_multiprogress_update(mp, verify,   i * 0.4, 100);
        usleep(25000);
    }

    sc_multiprogress_end(mp);
    return 0;
}
