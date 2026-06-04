#include "test_input.h"
#include "sparcli.h"
#include "input/input_internal.h"   /* sc_text_entry_frame */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


/** Strips ANSI codes from `line` and checks it contains `needle`. */
static bool line_contains(const char *line, const char *needle) {
    char *plain = sc_strip_ansi(line);
    bool found = plain && strstr(plain, needle) != NULL;
    free(plain);
    return found;
}

/** True when any rendered line of `frame` contains `needle` (ANSI-stripped). */
static bool frame_contains(const ScRendered *frame, const char *needle) {
    if (!frame) {
        return false;
    }
    for (size_t i = 0; i < frame->line_count; i++) {
        if (line_contains(frame->lines[i], needle)) {
            return true;
        }
    }
    return false;
}

static const char *const COMMANDS[] = {
    "commit", "checkout", "cherry-pick", "clone", "config", "push",
};
enum { N_COMMANDS = 6 };


static void check_prefix_matching(void);
static void check_fuzzy_matching(void);
static void check_highlight_and_overflow(void);
static void check_dropdown_inactive_cases(void);
static void check_boxed_dropdown(void);


/**
 * Non-interactive: dropdown autocomplete rendering via the snapshot frame
 * builder - matching, highlighting, overflow, and the inactive cases.
 */
void test_suggest(void) {
    check_prefix_matching();
    check_fuzzy_matching();
    check_highlight_and_overflow();
    check_dropdown_inactive_cases();
    check_boxed_dropdown();
}

/** Prefix mode lists prefix matches only and excludes exact matches. */
static void check_prefix_matching(void) {
    ScRendered *frame = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Cmd:", .initial = "ch",
        .suggestions = COMMANDS, .n_suggestions = N_COMMANDS,
        .suggest = { .mode = SC_SUGGEST_DROPDOWN },
    });
    CHECK(frame_contains(frame, "checkout"),
          "dropdown: prefix match 'ch' lists 'checkout'");
    CHECK(frame_contains(frame, "cherry-pick"),
          "dropdown: prefix match 'ch' lists 'cherry-pick'");
    CHECK(!frame_contains(frame, "commit"),
          "dropdown: non-prefix 'commit' is not listed");
    CHECK(!frame_contains(frame, "push"),
          "dropdown: non-prefix 'push' is not listed");
    sc_rendered_free(frame);

    /* An exact match leaves nothing to complete: the dropdown closes. */
    ScRendered *exact = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Cmd:", .initial = "push",
        .suggestions = COMMANDS, .n_suggestions = N_COMMANDS,
        .suggest = { .mode = SC_SUGGEST_DROPDOWN },
    });
    CHECK(!frame_contains(exact, "\xe2\x80\xa3"),   /* no ‣ marker row */
          "dropdown: exact match closes the dropdown");
    CHECK(!frame_contains(exact, "choose"),
          "dropdown: hint reverts to the default when closed");
    sc_rendered_free(exact);
}

/** Fuzzy mode matches subsequences and ranks the best match first. */
static void check_fuzzy_matching(void) {
    ScRendered *frame = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Cmd:", .initial = "cp",
        .suggestions = COMMANDS, .n_suggestions = N_COMMANDS,
        .suggest = { .mode = SC_SUGGEST_DROPDOWN,
                     .match = SC_SUGGEST_MATCH_FUZZY },
    });
    CHECK(frame_contains(frame, "cherry-pick"),
          "fuzzy: 'cp' matches 'cherry-pick' (subsequence)");
    CHECK(!frame_contains(frame, "checkout"),
          "fuzzy: 'cp' does not match 'checkout'");
    sc_rendered_free(frame);
}

/** Highlight marker, custom markers, and the "+N more" overflow line. */
static void check_highlight_and_overflow(void) {
    /* suggest_cursor is 1-based: 2 highlights the second visible row. */
    ScRendered *frame = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Cmd:", .initial = "c",
        .suggestions = COMMANDS, .n_suggestions = N_COMMANDS,
        .suggest = { .mode = SC_SUGGEST_DROPDOWN, .max_visible = 3,
                     .cursor_marker = ">> " },
        .suggest_cursor = 2,
    });
    CHECK(frame_contains(frame, ">> checkout"),
          "dropdown: cursor marker prefixes the highlighted row");
    CHECK(frame_contains(frame, "+2 more"),
          "dropdown: overflow line shows the hidden match count");
    sc_rendered_free(frame);

    /* The default hint advertises the dropdown keys. */
    ScRendered *hint = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Cmd:", .initial = "c",
        .suggestions = COMMANDS, .n_suggestions = N_COMMANDS,
        .suggest = { .mode = SC_SUGGEST_DROPDOWN },
    });
    CHECK(frame_contains(hint, "choose"),
          "dropdown: default hint shows the navigation keys");
    sc_rendered_free(hint);
}

/** Ghost mode and an empty value never render dropdown rows. */
static void check_dropdown_inactive_cases(void) {
    /* Ghost mode (zero-init): the suggestion appears as inline ghost text
     * appended to the value line (after the cursor block), never as a
     * separate dropdown row. */
    ScRendered *ghost = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Cmd:", .initial = "ch",
        .suggestions = COMMANDS, .n_suggestions = N_COMMANDS,
    });
    CHECK(ghost != NULL && ghost->line_count > 0
              && line_contains(ghost->lines[0], "eckout"),
          "ghost: suggestion completes inline on the value line");
    CHECK(!frame_contains(ghost, "cherry-pick"),
          "ghost: only the first match is shown, no list");
    sc_rendered_free(ghost);

    /* Empty value: no matches, no dropdown. */
    ScRendered *empty = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Cmd:",
        .suggestions = COMMANDS, .n_suggestions = N_COMMANDS,
        .suggest = { .mode = SC_SUGGEST_DROPDOWN },
    });
    CHECK(!frame_contains(empty, "commit"),
          "dropdown: empty value shows no suggestions");
    sc_rendered_free(empty);
}

/** Boxed mode stacks the dropdown beneath the panel. */
static void check_boxed_dropdown(void) {
    ScRendered *frame = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Cmd", .initial = "ch", .box.enabled = true, .box.width = 30,
        .suggestions = COMMANDS, .n_suggestions = N_COMMANDS,
        .suggest = { .mode = SC_SUGGEST_DROPDOWN,
                     .border = { .type = SC_BORDER_ROUNDED } },
        .suggest_cursor = 1,
    });
    /* The highlighted match row must sit inside the dropdown frame. */
    bool framed_row = false;
    for (size_t i = 0; frame && i < frame->line_count; i++) {
        if (line_contains(frame->lines[i], "checkout")
            && line_contains(frame->lines[i], "\xe2\x94\x82")) {   /* │ */
            framed_row = true;
        }
    }
    CHECK(framed_row,
          "boxed dropdown: match row renders inside the bordered frame");
    sc_rendered_free(frame);
}
