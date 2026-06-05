#include "sparcli.h"
#include <stdio.h>


void test_diff(void) {
    printf("\n");

    /* ── 1. basic change with context ── */
    printf("--- Diff 1. basic ---\n");
    {
        const char *old =
            "line one\nline two\nline three\nline four\nline five\n";
        const char *new =
            "line one\nline 2\nline three\nline four\nline five\n";
        sc_diff_print(old, new, (ScDiffOpts){ .context = 1 });
    }

    /* ── 2. additions + deletions ── */
    printf("\n--- Diff 2. add + delete ---\n");
    {
        const char *old = "alpha\nbeta\ngamma\ndelta\n";
        const char *new = "alpha\ngamma\ngamma2\ndelta\nepsilon\n";
        sc_diff_print(old, new, (ScDiffOpts){ 0 });
    }

    /* ── 3. identical → empty ── */
    printf("\n--- Diff 3. identical (no output expected) ---\n");
    {
        sc_diff_print("same\ntext\n", "same\ntext\n", (ScDiffOpts){ 0 });
        printf("[end]\n");
    }

    /* ── 4. no header, full context, custom labels ── */
    printf("\n--- Diff 4. no header, full ---\n");
    {
        const char *old = "a\nb\nc\n";
        const char *new = "a\nB\nc\n";
        sc_diff_print(old, new, (ScDiffOpts){
            .context = -1, .no_header = true });
    }

    /* ── 5. inside a panel ── */
    printf("\n--- Diff 5. framed ---\n");
    {
        ScRendered *r = sc_capture_diff(
            "key = 1\nname = old\n", "key = 2\nname = new\n",
            (ScDiffOpts){ .no_header = true, .context = -1 });
        sc_pad_print(r, (ScPadOpts){ .left = 2 });
        sc_rendered_free(r);
    }
}
