#include "test_input.h"

/* The line editor is internal; pull in its declarations directly. The
 * symbols live in libsparcli.a, so linking resolves them. */
#include "input/input_internal.h"

#include <string.h>


static ScKey kchar(const char *s) {
    ScKey k = { .type = SC_KEY_CHAR };
    strncpy(k.bytes, s, sizeof k.bytes - 1);
    k.codepoint = (unsigned char)s[0];
    return k;
}

static ScKey ktype(ScKeyType t) {
    ScKey k = { .type = t };
    return k;
}

void test_line_editor(void) {
    ScLineEditor e;
    sc_le_init(&e, "abc");
    CHECK(strcmp(e.buf, "abc") == 0 && e.len == 3 && e.cursor == 3,
          "init places cursor at end");

    /* Move left once and insert: 'ab' | 'c' → 'abXc'. */
    sc_le_handle(&e, ktype(SC_KEY_LEFT));
    sc_le_handle(&e, kchar("X"));
    CHECK(strcmp(e.buf, "abXc") == 0 && e.cursor == 3, "insert before cursor");

    /* Backspace removes the char before the cursor: 'abXc' → 'abc'. */
    sc_le_handle(&e, ktype(SC_KEY_BACKSPACE));
    CHECK(strcmp(e.buf, "abc") == 0 && e.cursor == 2, "backspace");

    /* Home, then delete-forward removes 'a'. */
    sc_le_handle(&e, ktype(SC_KEY_HOME));
    CHECK(e.cursor == 0, "home moves to start");
    sc_le_handle(&e, ktype(SC_KEY_DELETE));
    CHECK(strcmp(e.buf, "bc") == 0 && e.cursor == 0, "delete forward");

    /* Ctrl-E to end, Ctrl-U kills everything before cursor. */
    sc_le_handle(&e, ktype(SC_KEY_CTRL_E));
    sc_le_handle(&e, ktype(SC_KEY_CTRL_U));
    CHECK(strcmp(e.buf, "") == 0 && e.len == 0, "Ctrl-U clears to start");

    /* Multi-byte insert keeps cursor on a codepoint boundary. */
    sc_le_handle(&e, kchar("é"));   /* 0xC3 0xA9 */
    CHECK(e.len == 2 && e.cursor == 2, "multi-byte insert advances by 2 bytes");
    sc_le_handle(&e, ktype(SC_KEY_BACKSPACE));
    CHECK(e.len == 0, "backspace deletes whole codepoint");

    /* Word-kill: 'foo bar' with cursor at end removes 'bar'. */
    sc_le_free(&e);
    sc_le_init(&e, "foo bar");
    sc_le_handle(&e, ktype(SC_KEY_CTRL_W));
    CHECK(strcmp(e.buf, "foo ") == 0, "Ctrl-W deletes the last word");

    sc_le_free(&e);
}
