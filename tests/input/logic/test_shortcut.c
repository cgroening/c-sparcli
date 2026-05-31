#include "test_input.h"
#include "sparcli.h"


/**
 * Pure tests for the shortcut chord builders and matcher (no TTY needed).
 * Verifies the named-ctrl vs CHAR+CTRL normalization, function keys, Alt, and
 * that an unmodified key never matches a Ctrl/Alt chord.
 */
void test_shortcut(void) {
    ScKey ctrl_e  = { .type = SC_KEY_CTRL_E };
    ScKey ctrl_o  = { .type = SC_KEY_CHAR, .codepoint = 'o', .mods = SC_MOD_CTRL };
    ScKey f2      = { .type = SC_KEY_F2 };
    ScKey alt_e   = { .type = SC_KEY_CHAR, .codepoint = 'e', .mods = SC_MOD_ALT };
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
}
