#include "input/sparcli_shortcut.h"
#include "input/input_internal.h"
#include "internal.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>


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
    // Character chords are CASE-SENSITIVE (`p` != `P`); only Ctrl folds case,
    // because the terminal sends the same byte for Ctrl+p and Ctrl+P.
    *out_cp = (type == SC_KEY_CHAR)
            ? ((mods & SC_MOD_CTRL) ? lower_cp(cp) : cp) : 0;
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

ScKeyChord sc_key_special(ScKeyType key) {
    return (ScKeyChord){ .key = key };
}

ScKeyChord sc_key_mod(ScKeyType key, uint8_t mods) {
    return (ScKeyChord){ .key = key, .mods = mods };
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

/** Base (modifier-free) name of a normalized key into `buf`. */
static void key_base_name(ScKeyType type, uint32_t cp, bool ctrl,
                          char *buf, size_t cap) {
    if (type >= SC_KEY_F1 && type <= SC_KEY_F12) {
        snprintf(buf, cap, "F%d", (int)(type - SC_KEY_F1 + 1));
        return;
    }
    if (type == SC_KEY_CHAR) {
        if (cp == ' ') {
            snprintf(buf, cap, "\xe2\x90\xa3"); /* ␣ open box for the space key */
            return;
        }
        char letter = (char)cp;
        if (ctrl) { letter = (char)toupper((unsigned char)letter); }
        snprintf(buf, cap, "%c", letter);
        return;
    }
    const char *name;
    switch (type) {
        case SC_KEY_LEFT:      name = "\xe2\x86\x90"; break; /* ← */
        case SC_KEY_RIGHT:     name = "\xe2\x86\x92"; break; /* → */
        case SC_KEY_UP:        name = "\xe2\x86\x91"; break; /* ↑ */
        case SC_KEY_DOWN:      name = "\xe2\x86\x93"; break; /* ↓ */
        case SC_KEY_ENTER:     name = "\xe2\x86\xb5"; break; /* ↵ */
        case SC_KEY_TAB:       name = "\xe2\x87\xa5"; break; /* ⇥ */
        case SC_KEY_BACKTAB:   name = "\xe2\x87\xa4"; break; /* ⇤ */
        case SC_KEY_BACKSPACE: name = "Bksp"; break;
        case SC_KEY_DELETE:    name = "Del";  break;
        case SC_KEY_HOME:      name = "Home"; break;
        case SC_KEY_END:       name = "End";  break;
        case SC_KEY_PAGEUP:    name = "PgUp"; break;
        case SC_KEY_PAGEDOWN:  name = "PgDn"; break;
        case SC_KEY_ESC:       name = "Esc";  break;
        default:               name = "?";    break;
    }
    snprintf(buf, cap, "%s", name);
}

void sc_key_chord_name(ScKeyChord chord, char *buf, size_t cap) {
    if (!buf || cap == 0) {
        return;
    }
    ScKeyType nt;
    uint32_t ncp;
    uint8_t nm;
    normalize(chord.key, chord.codepoint, chord.mods, &nt, &ncp, &nm);

    char base[16];
    key_base_name(nt, ncp, (nm & SC_MOD_CTRL) != 0, base, sizeof base);

    // Modifier prefix: Ctrl keeps the established `^`; Alt `M-`; Shift `S-`.
    char prefix[8];
    size_t p = 0;
    if (nm & SC_MOD_CTRL)  { prefix[p++] = '^'; }
    if (nm & SC_MOD_ALT)   { prefix[p++] = 'M'; prefix[p++] = '-'; }
    if (nm & SC_MOD_SHIFT) { prefix[p++] = 'S'; prefix[p++] = '-'; }
    prefix[p] = '\0';

    snprintf(buf, cap, "%s%s", prefix, base);
}

int sc_shortcut_hint_rows(const ScShortcut *items, size_t n,
                          int indent, int term_w) {
    if (!items) {
        return 0;
    }
    if (term_w < 1) {
        term_w = 1;
    }
    // Visible width of the one-line footer, matching build_shortcut_hint():
    // `name SP label`, joined by "  ·  " (5 columns), after `indent` columns.
    int width = indent > 0 ? indent : 0;
    int shown = 0;
    for (size_t i = 0; i < n; i++) {
        const ScShortcut *s = &items[i];
        if (!s->hint_label || !s->hint_label[0]) {
            continue;
        }
        if (shown > 0) { width += 5; }            /* "  ·  " */
        char name[16];
        sc_key_chord_name(s->chord, name, sizeof name);
        width += (int)sc_utf8_string_length(name, strlen(name));
        width += 1;                               /* space before the label */
        width += (int)sc_utf8_string_length(s->hint_label,
                                            strlen(s->hint_label));
        shown++;
    }
    if (shown == 0) {
        return 0;
    }
    int rows = (width + term_w - 1) / term_w;     /* ceil: terminal soft-wrap */
    return rows < 1 ? 1 : rows;
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
