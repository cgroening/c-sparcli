#include "test_input.h"
#include "sparcli.h"

#include <string.h>


/** Decodes the whole of `seq` and checks the first key's type + byte count. */
static void expect(const char *seq, ScKeyType type, size_t consumed,
                   const char *label) {
    ScKey  k;
    size_t used = sc_key_decode(seq, strlen(seq), &k);
    CHECK(k.type == type && used == consumed, label);
}

void test_key_decode(void) {
    expect("a",       SC_KEY_CHAR,      1, "printable 'a'");
    expect("\r",      SC_KEY_ENTER,     1, "carriage return → Enter");
    expect("\n",      SC_KEY_ENTER,     1, "newline → Enter");
    expect("\t",      SC_KEY_TAB,       1, "tab");
    expect("\x7f",    SC_KEY_BACKSPACE, 1, "DEL → Backspace");
    expect("\x03",    SC_KEY_CTRL_C,    1, "Ctrl-C");
    expect("\x15",    SC_KEY_CTRL_U,    1, "Ctrl-U");

    expect("\x1bx",   SC_KEY_ESC,       1, "ESC + non-CSI byte → Esc");
    expect("\x1b[A",  SC_KEY_UP,        3, "CSI up");
    expect("\x1b[B",  SC_KEY_DOWN,      3, "CSI down");
    expect("\x1b[C",  SC_KEY_RIGHT,     3, "CSI right");
    expect("\x1b[D",  SC_KEY_LEFT,      3, "CSI left");
    expect("\x1b[H",  SC_KEY_HOME,      3, "CSI home");
    expect("\x1b[F",  SC_KEY_END,       3, "CSI end");
    expect("\x1b[Z",  SC_KEY_BACKTAB,   3, "CSI shift-tab");
    expect("\x1b[3~", SC_KEY_DELETE,    4, "CSI delete");
    expect("\x1b[5~", SC_KEY_PAGEUP,    4, "CSI page-up");
    expect("\x1b[6~", SC_KEY_PAGEDOWN,  4, "CSI page-down");
    expect("\x1bOH",  SC_KEY_HOME,      3, "SS3 home");

    /* Multi-byte UTF-8: 'é' = 0xC3 0xA9, one codepoint, two bytes. */
    ScKey k;
    size_t used = sc_key_decode("\xc3\xa9", 2, &k);
    CHECK(k.type == SC_KEY_CHAR && used == 2 && k.codepoint == 0xE9,
          "UTF-8 'é' decodes to one codepoint");

    /* Incomplete multi-byte: only the lead byte present → needs more. */
    used = sc_key_decode("\xc3", 1, &k);
    CHECK(used == 0 && k.type == SC_KEY_NONE,
          "incomplete UTF-8 reports 0 consumed");

    /* A lone ESC is an incomplete prefix at the decoder level: the buffered
     * reader (sc_tty_read_key) resolves it to Esc via a short poll timeout. */
    used = sc_key_decode("\x1b", 1, &k);
    CHECK(used == 0 && k.type == SC_KEY_NONE,
          "lone ESC reports 0 consumed (reader times out to Esc)");
}
