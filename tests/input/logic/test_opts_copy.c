#include "test_input.h"
#include "sparcli.h"
#include "input/input_internal.h"   /* sc_fuzzy_frame / sc_select_frame */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Strips ANSI codes from `line` and checks it contains `needle`. */
static bool line_contains(const char *line, const char *needle) {
    char *plain = sc_strip_ansi(line);
    bool found = plain && strstr(plain, needle) != NULL;
    free(plain);
    return found;
}

/**
 * Non-interactive: `sc_fuzzy_new` / `sc_select_new` copy the opts string
 * fields, so they stay valid after the caller's buffers are gone (e.g.
 * temporary strings handed over by an FFI binding).
 */
void test_opts_copy(void) {
    /* Fuzzy: clobber the caller's prompt buffer after construction. */
    char prompt[32];
    snprintf(prompt, sizeof prompt, "Category");
    ScFuzzy *fuzzy = sc_fuzzy_new((ScFuzzyOpts){ .prompt = prompt });
    memset(prompt, 'X', sizeof prompt - 1);
    prompt[sizeof prompt - 1] = '\0';

    sc_fuzzy_add(fuzzy, "Groceries");
    ScRendered *frame = sc_fuzzy_frame(fuzzy, "");
    CHECK(frame != NULL && frame->line_count > 0, "fuzzy: frame renders");
    if (frame && frame->line_count > 0) {
        CHECK(line_contains(frame->lines[0], "Category"),
              "fuzzy: prompt survives caller buffer reuse");
        CHECK(!line_contains(frame->lines[0], "XXX"),
              "fuzzy: clobbered caller buffer is not rendered");
    }
    sc_rendered_free(frame);
    sc_fuzzy_free(fuzzy);

    /* Select: same contract for the prompt. */
    char select_prompt[32];
    snprintf(select_prompt, sizeof select_prompt, "Pick one");
    ScSelect *select =
        sc_select_new((ScSelectOpts){ .prompt = select_prompt });
    memset(select_prompt, 'Y', sizeof select_prompt - 1);
    select_prompt[sizeof select_prompt - 1] = '\0';

    sc_select_add(select, "Apple");
    ScRendered *select_frame = sc_select_frame(select);
    CHECK(select_frame != NULL && select_frame->line_count > 0,
          "select: frame renders");
    if (select_frame && select_frame->line_count > 0) {
        CHECK(line_contains(select_frame->lines[0], "Pick one"),
              "select: prompt survives caller buffer reuse");
    }
    sc_rendered_free(select_frame);
    sc_select_free(select);

    /* Theme: glyph strings are copied into the process-wide theme. */
    char marker[16];
    snprintf(marker, sizeof marker, ">> ");
    sc_input_set_theme(&(ScInputTheme){ .cursor_marker = marker });
    memset(marker, 'Z', sizeof marker - 1);
    marker[sizeof marker - 1] = '\0';

    ScSelect *themed = sc_select_new((ScSelectOpts){ 0 });
    sc_select_add(themed, "Apple");
    ScRendered *themed_frame = sc_select_frame(themed);
    CHECK(themed_frame != NULL && themed_frame->line_count > 0,
          "theme: frame renders");
    if (themed_frame && themed_frame->line_count > 0) {
        CHECK(line_contains(themed_frame->lines[0], ">> "),
              "theme: cursor marker survives caller buffer reuse");
    }
    sc_rendered_free(themed_frame);
    sc_select_free(themed);
    sc_input_set_theme(NULL);   // reset so later tests see no theme
}
