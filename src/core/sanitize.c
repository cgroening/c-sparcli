#include "sparcli.h"
#include "core/sanitize_internal.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


/* ── ECMA-48 byte classes ────────────────────────────────────────────────── */

/** Escape character that introduces every ANSI sequence. */
#define ANSI_ESC 0x1b

/** BEL terminates an OSC sequence (xterm convention). */
#define ANSI_BEL 0x07

/** DEL is dropped like the C0 control bytes. */
#define ANSI_DEL 0x7f

/** CSI parameter/intermediate bytes: `0x20`-`0x3F` (`0;38;2;…`, `?`, …). */
#define CSI_BODY_MIN 0x20
#define CSI_BODY_MAX 0x3f

/** CSI final byte range (`m`, `A`, `H`, …). */
#define CSI_FINAL_MIN 0x40
#define CSI_FINAL_MAX 0x7e

/** Intermediate bytes of a non-CSI escape sequence (`ESC ( B`). */
#define ESC_INTERMEDIATE_MIN 0x20
#define ESC_INTERMEDIATE_MAX 0x2f

/** Final byte range of a non-CSI escape sequence (`ESC c`, `ESC 7`). */
#define ESC_FINAL_MIN 0x30
#define ESC_FINAL_MAX 0x7e


static const char *skip_csi(const char *p);
static const char *skip_osc(const char *p);
static const char *skip_string_sequence(const char *p);
static const char *skip_escape_sequence(const char *p);
static bool is_dropped_control_byte(unsigned char byte);


/*
 * Process-wide ANSI passthrough default. Atomic (relaxed) so concurrent
 * renderers may read it while another thread sets it without a data race;
 * it is set-once configuration with no ordering dependencies.
 */
static atomic_bool g_allow_ansi = false;


void sc_set_allow_ansi(bool allow) {
    atomic_store_explicit(&g_allow_ansi, allow, memory_order_relaxed);
}

bool sc_allow_ansi(void) {
    return atomic_load_explicit(&g_allow_ansi, memory_order_relaxed);
}

bool sc_ansi_mode_resolve(ScAnsiMode mode) {
    if (mode == SC_ANSI_MODE_ALLOW)    { return true; }
    if (mode == SC_ANSI_MODE_SANITIZE) { return false; }
    return sc_allow_ansi();
}

char *sc_sanitize_copy(const char *str, bool allow_ansi) {
    if (!str) { return NULL; }

    /* Output is never longer than the input. */
    size_t input_length = strlen(str);
    char *output = malloc(input_length + 1);
    if (!output) { return NULL; }

    size_t write_position = 0;
    size_t read_position = 0;
    while (read_position < input_length) {
        unsigned char current_byte = (unsigned char)str[read_position];

        if (current_byte == ANSI_ESC) {
            const char *sequence_end = sc_ansi_skip_seq(str + read_position);
            size_t sequence_length =
                (size_t)(sequence_end - (str + read_position));
            if (sequence_length == 0) {
                read_position++;  /* lone/malformed ESC: drop the byte */
                continue;
            }
            if (allow_ansi) {
                memcpy(output + write_position, str + read_position,
                       sequence_length);
                write_position += sequence_length;
            }
            read_position += sequence_length;
            continue;
        }

        if (is_dropped_control_byte(current_byte)) {
            read_position++;
            continue;
        }
        output[write_position++] = str[read_position++];
    }
    output[write_position] = '\0';
    return output;
}

char *sc_sanitize_copy_mode(const char *str, ScAnsiMode mode) {
    return sc_sanitize_copy(str, sc_ansi_mode_resolve(mode));
}

const char *sc_ansi_skip_seq(const char *p) {
    if ((unsigned char)p[0] != ANSI_ESC) { return p; }

    unsigned char introducer = (unsigned char)p[1];
    switch (introducer) {
        case '[': return skip_csi(p);
        case ']': return skip_osc(p);
        case 'P': case 'X': case '^': case '_':
            return skip_string_sequence(p);
        default:
            return skip_escape_sequence(p);
    }
}

/**
 * Skips a CSI sequence (`ESC [` body bytes, final byte). Returns `p` when
 * the final byte is missing (unterminated/malformed).
 */
static const char *skip_csi(const char *p) {
    const unsigned char *cursor = (const unsigned char *)p + 2;
    while (*cursor >= CSI_BODY_MIN && *cursor <= CSI_BODY_MAX) { cursor++; }
    if (*cursor >= CSI_FINAL_MIN && *cursor <= CSI_FINAL_MAX) {
        return (const char *)cursor + 1;
    }
    return p;
}

/**
 * Skips an OSC sequence (`ESC ]` … BEL or ST). Returns `p` when
 * unterminated.
 */
static const char *skip_osc(const char *p) {
    const unsigned char *cursor = (const unsigned char *)p + 2;
    while (*cursor) {
        if (*cursor == ANSI_BEL) { return (const char *)cursor + 1; }
        if (*cursor == ANSI_ESC && cursor[1] == '\\') {
            return (const char *)cursor + 2;
        }
        cursor++;
    }
    return p;
}

/**
 * Skips a DCS/SOS/PM/APC string sequence (`ESC P/X/^/_` … ST). Returns
 * `p` when unterminated.
 */
static const char *skip_string_sequence(const char *p) {
    const unsigned char *cursor = (const unsigned char *)p + 2;
    while (*cursor) {
        if (*cursor == ANSI_ESC && cursor[1] == '\\') {
            return (const char *)cursor + 2;
        }
        cursor++;
    }
    return p;
}

/**
 * Skips a non-CSI escape sequence (`ESC` intermediates final, e.g.
 * `ESC c`, `ESC ( B`). Returns `p` when no valid final byte follows.
 */
static const char *skip_escape_sequence(const char *p) {
    const unsigned char *cursor = (const unsigned char *)p + 1;
    while (*cursor >= ESC_INTERMEDIATE_MIN && *cursor <= ESC_INTERMEDIATE_MAX) {
        cursor++;
    }
    if (*cursor >= ESC_FINAL_MIN && *cursor <= ESC_FINAL_MAX) {
        return (const char *)cursor + 1;
    }
    return p;
}

/** Returns `true` for control bytes that are always removed. */
static bool is_dropped_control_byte(unsigned char byte) {
    if (byte == '\n' || byte == '\t') { return false; }
    return byte < 0x20 || byte == ANSI_DEL;
}
