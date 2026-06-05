/*
 * diff.c - colored unified diff of two texts.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/diff
 */

#include <sparcli.h>

#include <stdio.h>


int main(void) {
    const char *before =
        "name = sparcli\n"
        "version = 0.1.0\n"
        "color = blue\n"
        "debug = false\n";
    const char *after =
        "name = sparcli\n"
        "version = 0.2.0\n"
        "color = green\n"
        "debug = false\n"
        "verbose = true\n";

    sc_diff_print(before, after, (ScDiffOpts){
        .old_label = "config.toml (old)",
        .new_label = "config.toml (new)",
        .context = 1,
    });

    printf("\n");

    // The same diff framed in a panel via the capture API.
    ScRendered *r = sc_capture_diff(before, after,
                                    (ScDiffOpts){ .no_header = true });
    sc_pad_print(r, (ScPadOpts){ .left = 2 });
    sc_rendered_free(r);
    return 0;
}
