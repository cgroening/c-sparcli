#include "test_input.h"
#include "sparcli.h"
#include "input/input_internal.h"   /* sc_shortcut_hint_rows */

#include <string.h>


/**
 * Pure tests for the shortcut chord builders and matcher (no TTY needed).
 * Verifies the named-ctrl vs CHAR+CTRL normalization, function keys, Alt, and
 * that an unmodified key never matches a Ctrl/Alt chord.
 */
void test_shortcut(void) {
    ScKey ctrl_e  = { .type = SC_KEY_CTRL_E };
    ScKey ctrl_o  = {
        .type = SC_KEY_CHAR, .codepoint = 'o', .mods = SC_MOD_CTRL
    };
    ScKey f2      = { .type = SC_KEY_F2 };
    ScKey alt_e   = {
        .type = SC_KEY_CHAR, .codepoint = 'e', .mods = SC_MOD_ALT
    };
    ScKey plain_e = { .type = SC_KEY_CHAR, .codepoint = 'e', .mods = 0 };

    CHECK(sc_key_chord_matches(ctrl_e, sc_key_ctrl('e')),
          "named Ctrl-E matches sc_key_ctrl('e')");
    CHECK(sc_key_chord_matches(ctrl_e, sc_key_ctrl('E')),
          "Ctrl chord is case-insensitive");
    CHECK(sc_key_chord_matches(ctrl_o, sc_key_ctrl('o')),
          "generic Ctrl-O (CHAR+CTRL) matches sc_key_ctrl('o')");
    CHECK(sc_key_chord_matches(f2, sc_key_fn(2)), "F2 matches sc_key_fn(2)");
    CHECK(!sc_key_chord_matches(f2, sc_key_fn(3)), "F2 does not match F3");
    CHECK(sc_key_chord_matches(alt_e, sc_key_alt('e')),
          "Alt+e matches sc_key_alt('e')");
    CHECK(!sc_key_chord_matches(plain_e, sc_key_ctrl('e')),
          "plain 'e' does not match Ctrl-E");
    CHECK(!sc_key_chord_matches(plain_e, sc_key_alt('e')),
          "plain 'e' does not match Alt-e");

    ScShortcut items[] = {
        { .chord = sc_key_fn(2),     .id = 7, .mode = SC_SHORTCUT_RETURN },
        { .chord = sc_key_ctrl('o'), .id = 9, .mode = SC_SHORTCUT_RETURN },
    };
    const ScShortcut *hit = sc_shortcut_find(f2, items, 2);
    CHECK(hit && hit->id == 7, "find returns the F2 binding");
    hit = sc_shortcut_find(ctrl_o, items, 2);
    CHECK(hit && hit->id == 9, "find returns the Ctrl-O binding");
    hit = sc_shortcut_find(plain_e, items, 2);
    CHECK(hit == NULL, "find returns NULL when nothing matches");

    /* Arrow/Enter chords (carrying a plain special key) match and render as
       their glyph - used by the CLI's --arrow-nav. */
    ScKey      left       = { .type = SC_KEY_LEFT };
    ScKeyChord left_chord  = { .key = SC_KEY_LEFT };
    ScKeyChord right_chord = { .key = SC_KEY_RIGHT };
    CHECK(sc_key_chord_matches(left, left_chord),
          "Left key matches a Left chord");
    CHECK(!sc_key_chord_matches(left, right_chord),
          "Left key does not match a Right chord");

    char name[16] = { 0 };
    sc_key_chord_name(left_chord, name, sizeof name);
    CHECK(strcmp(name, "\xe2\x86\x90") == 0, "Left chord renders as ←");
    sc_key_chord_name(right_chord, name, sizeof name);
    CHECK(strcmp(name, "\xe2\x86\x92") == 0, "Right chord renders as →");
    sc_key_chord_name(sc_key_fn(2), name, sizeof name);
    CHECK(strcmp(name, "F2") == 0, "F-key chord still renders as Fn");

    /* Named non-arrow keys get a word; modifiers prefix the base name. */
    sc_key_chord_name((ScKeyChord){ .key = SC_KEY_DELETE }, name, sizeof name);
    CHECK(strcmp(name, "Del") == 0, "Delete chord renders as Del (not ?)");
    sc_key_chord_name((ScKeyChord){ .key = SC_KEY_BACKSPACE }, name, sizeof name);
    CHECK(strcmp(name, "Bksp") == 0, "Backspace chord renders as Bksp");
    sc_key_chord_name((ScKeyChord){ .key = SC_KEY_UP, .mods = SC_MOD_ALT },
                      name, sizeof name);
    CHECK(strcmp(name, "M-\xe2\x86\x91") == 0, "Alt+Up renders as M-↑ (not M-?)");
    sc_key_chord_name(
        (ScKeyChord){ .key = SC_KEY_UP, .mods = SC_MOD_ALT | SC_MOD_SHIFT },
        name, sizeof name);
    CHECK(strcmp(name, "M-S-\xe2\x86\x91") == 0, "Alt+Shift+Up renders as M-S-↑");
    sc_key_chord_name(sc_key_ctrl('x'), name, sizeof name);
    CHECK(strcmp(name, "^X") == 0, "Ctrl-X still renders as ^X");
    sc_key_chord_name((ScKeyChord){ .key = SC_KEY_CHAR, .codepoint = ' ' },
                      name, sizeof name);
    CHECK(strcmp(name, "\xe2\x90\xa3") == 0, "Space chord renders as ␣");

    /* Footer height accounts for terminal soft-wrap: a footer wider than the
       terminal wraps to several rows; no labels = 0 rows. */
    ScShortcut foot[] = {
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'd' }, .hint_label = "done" },
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'n' }, .hint_label = "new" },
    };
    /* "d done  ·  n new" = 16 cols. */
    CHECK(sc_shortcut_hint_rows(foot, 2, 0, 80) == 1, "footer fits 80 cols = 1 row");
    CHECK(sc_shortcut_hint_rows(foot, 2, 0, 10) == 2, "footer wraps at 10 cols = 2 rows");
    CHECK(sc_shortcut_hint_rows(foot, 2, 6, 10) == 3,
          "footer + indent 6 wraps at 10 cols = 3 rows");
    ScShortcut none[] = {
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'd' } },   /* no label */
    };
    CHECK(sc_shortcut_hint_rows(none, 1, 0, 80) == 0, "no labels = 0 rows");

    /* Char shortcuts are case-sensitive: 'P' (Shift+p) != 'p'. */
    ScKey      pressed_P = { .type = SC_KEY_CHAR, .codepoint = 'P' };
    ScKeyChord chord_p   = { .key = SC_KEY_CHAR, .codepoint = 'p' };
    ScKeyChord chord_P   = { .key = SC_KEY_CHAR, .codepoint = 'P' };
    CHECK(!sc_key_chord_matches(pressed_P, chord_p),
          "case-sensitive: 'P' does not match a 'p' chord");
    CHECK(sc_key_chord_matches(pressed_P, chord_P),
          "case-sensitive: 'P' matches a 'P' chord");

    /* Modified named keys via sc_key_mod: Shift/Alt + arrows etc. */
    ScKey shift_up = { .type = SC_KEY_UP, .mods = SC_MOD_SHIFT };
    CHECK(sc_key_chord_matches(shift_up, sc_key_mod(SC_KEY_UP, SC_MOD_SHIFT)),
          "Shift+Up matches sc_key_mod(UP, SHIFT)");
    CHECK(!sc_key_chord_matches(shift_up, sc_key_special(SC_KEY_UP)),
          "Shift+Up does not match a plain Up chord");
    ScKey alt_shift_down = {
        .type = SC_KEY_DOWN, .mods = SC_MOD_ALT | SC_MOD_SHIFT
    };
    CHECK(sc_key_chord_matches(
              alt_shift_down,
              sc_key_mod(SC_KEY_DOWN, SC_MOD_ALT | SC_MOD_SHIFT)),
          "Alt+Shift+Down matches sc_key_mod(DOWN, ALT|SHIFT)");
    sc_key_chord_name(sc_key_mod(SC_KEY_HOME, SC_MOD_SHIFT), name, sizeof name);
    CHECK(strcmp(name, "S-Home") == 0, "sc_key_mod(HOME, SHIFT) renders S-Home");
}
