/*
 * subprocess.c - run an external command (no shell) and capture its output.
 *
 * Run (after `make`):
 *   make run-example EX=c/app/subprocess
 */

#include <sparcli.h>

#include <stdio.h>


int main(void) {
    // Capture stdout + exit code.
    const char *argv[] = { "uname", "-sm", NULL };
    ScProcResult r = sc_run(argv, (ScProcOpts){ 0 });
    if (r.ok) {
        printf("uname -sm  → exit %d\n  %s", r.exit_code, r.out);
    }
    sc_proc_result_free(&r);

    // Feed stdin through a filter, capture the result.
    const char *wc[] = { "wc", "-w", NULL };
    ScProcResult words = sc_run(wc, (ScProcOpts){
        .input = "one two three four five\n",
    });
    printf("wc -w      → %s", words.out);
    sc_proc_result_free(&words);

    // A missing command reports exit 127 (not a crash).
    const char *missing[] = { "definitely-not-a-real-binary", NULL };
    ScProcResult m = sc_run(missing, (ScProcOpts){ 0 });
    printf("missing    → exit %d\n", m.exit_code);
    sc_proc_result_free(&m);
    return 0;
}
