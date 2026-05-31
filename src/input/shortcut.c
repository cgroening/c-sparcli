#include "input/sparcli_shortcut.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>


/** Lowercases an ASCII letter codepoint; leaves everything else unchanged. */
static uint32_t lower_cp(uint32_t cp) {
    return (cp >= 'A' && cp <= 'Z') ? cp + 32 : cp;
}

/**
 * Collapses a key/codepoint/mods triple into a canonical form so the named
 * `SC_KEY_CTRL_*` keys and the generic `SC_KEY_CHAR + SC_MOD_CTRL` encoding
 * compare equal. Letters are lowercased.
 */
static void normalize(
    ScKeyType type, uint32_t cp, uint8_t mods,
    ScKeyType *out_type, uint32_t *out_cp, uint8_t *out_mods
) {
    char letter = 0;
    switch (type) {
        case SC_KEY_CTRL_A: letter = 'a'; break;
        case SC_KEY_CTRL_C: letter = 'c'; break;
        case SC_KEY_CTRL_D: letter = 'd'; break;
        case SC_KEY_CTRL_E: letter = 'e'; break;
        case SC_KEY_CTRL_K: letter = 'k'; break;
        case SC_KEY_CTRL_U: letter = 'u'; break;
        case SC_KEY_CTRL_W: letter = 'w'; break;
        default: break;
    }
    if (letter) {
        *out_type = SC_KEY_CHAR;
        *out_cp = (uint32_t)letter;
        *out_mods = mods | SC_MOD_CTRL;
        return;
    }
    *out_type = type;
    *out_cp = (type == SC_KEY_CHAR) ? lower_cp(cp) : 0;
    *out_mods = mods;
}

ScKeyChord sc_key_ctrl(char letter) {
    char lower = (char)tolower((unsigned char)letter);
    // Not bindable: Ctrl-C is cancel; Ctrl-H/I/J/M arrive as
    // Backspace/Tab/Enter, never as a distinct Ctrl chord.
    assert(lower >= 'a' && lower <= 'z' && "Ctrl shortcut needs a letter");
    assert(lower != 'c' && lower != 'h' && lower != 'i'
           && lower != 'j' && lower != 'm'
           && "this Ctrl-letter is reserved / not deliverable");
    return (ScKeyChord){
        .key = SC_KEY_CHAR, .codepoint = (uint32_t)lower, .mods = SC_MOD_CTRL
    };
}

ScKeyChord sc_key_fn(int n) {
    assert(n >= 1 && n <= 12 && "function key out of range (F1..F12)");
    return (ScKeyChord){
        .key = (ScKeyType)(SC_KEY_F1 + (n - 1)), .codepoint = 0, .mods = 0
    };
}

ScKeyChord sc_key_alt(char letter) {
    char lower = (char)tolower((unsigned char)letter);
    assert(lower >= 'a' && lower <= 'z' && "Alt shortcut needs a letter");
    return (ScKeyChord){
        .key = SC_KEY_CHAR, .codepoint = (uint32_t)lower, .mods = SC_MOD_ALT
    };
}

bool sc_key_chord_matches(ScKey key, ScKeyChord chord) {
    ScKeyType kt, ct;
    uint32_t kc, cc;
    uint8_t km, cm;
    normalize(key.type, key.codepoint, key.mods, &kt, &kc, &km);
    normalize(chord.key, chord.codepoint, chord.mods, &ct, &cc, &cm);

    if (kt != ct || km != cm) {
        return false;
    }
    if (kt == SC_KEY_CHAR) {
        return kc == cc;
    }
    return true;   // function keys (and any other named key) match by type
}

void sc_key_chord_name(ScKeyChord chord, char *buf, size_t cap) {
    if (!buf || cap == 0) {
        return;
    }
    if (chord.key >= SC_KEY_F1 && chord.key <= SC_KEY_F12) {
        snprintf(buf, cap, "F%d", (int)(chord.key - SC_KEY_F1 + 1));
        return;
    }
    ScKeyType nt;
    uint32_t ncp;
    uint8_t nm;
    normalize(chord.key, chord.codepoint, chord.mods, &nt, &ncp, &nm);
    char letter = (nt == SC_KEY_CHAR) ? (char)ncp : '?';
    if (nm & SC_MOD_CTRL) {
        snprintf(buf, cap, "^%c", toupper((unsigned char)letter));
    } else if (nm & SC_MOD_ALT) {
        snprintf(buf, cap, "M-%c", letter);
    } else {
        snprintf(buf, cap, "%c", letter);
    }
}

const ScShortcut *sc_shortcut_find(
    ScKey key, const ScShortcut *items, size_t n
) {
    if (!items) {
        return NULL;
    }
    for (size_t i = 0; i < n; i++) {
        if (sc_key_chord_matches(key, items[i].chord)) {
            return &items[i];
        }
    }
    return NULL;
}
