#include "tty_internal.h"

#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>


static size_t decode_escape(const char *buf, size_t len, ScKey *out);
    static size_t decode_alt_prefix(const char *buf, size_t len, ScKey *out);
    static size_t decode_ss3(char final, ScKey *out);
    static void decode_csi_tilde(int param, int modifier, ScKey *out);
    static bool decode_csi_letter(char final, ScKey *out);
    static void make_char(ScKey *out, const char *bytes, size_t seq_len);
    static size_t utf8_seq_len(unsigned char lead);


/* Buffered, not-yet-decoded input bytes, persisted across read calls. */
static char g_buf[64];
static size_t g_buf_len = 0;


ScKey sc_tty_read_key(void) {
    const ScKey none = { .type = SC_KEY_NONE, .codepoint = 0, .bytes = {0} };
    const ScKey resize = {
        .type = SC_KEY_RESIZE, .codepoint = 0, .bytes = {0}
    };

    int fd = sc_tty_internal_fd();
    for (;;) {
        ScKey key;
        size_t used = sc_key_decode(g_buf, g_buf_len, &key);
        if (used > 0) {
            memmove(g_buf, g_buf + used, g_buf_len - used);
            g_buf_len -= used;
            return key;
        }
        // A lone ESC is ambiguous: it may begin an escape sequence whose
        // remaining bytes are still in flight, or it may be the user pressing
        // Escape. Wait briefly; if nothing more arrives, treat it as Esc.
        //
        // Uses select() rather than poll(): on macOS poll() on /dev/tty is
        // broken (it reports POLLNVAL instead of timing out), which would make
        // the reader fall through to a blocking read() and swallow a lone Esc
        // until the next key. select() works on /dev/tty there.
        if (g_buf_len > 0 && (unsigned char)g_buf[0] == 0x1b) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(fd, &read_fds);
            struct timeval timeout = { .tv_sec = 0, .tv_usec = 25 * 1000 };
            int select_result = select(fd + 1, &read_fds, NULL, NULL, &timeout);
            if (select_result == 0) {
                memmove(g_buf, g_buf + 1, g_buf_len - 1);
                g_buf_len -= 1;
                ScKey esc = {
                    .type = SC_KEY_ESC, .codepoint = 0, .bytes = {0}
                };
                return esc;
            }
            if (select_result < 0 && errno != EINTR) {
                return none;
            }
            if (select_result < 0 && sc_tty_take_resize()) {
                return resize;
            }
            // EINTR without resize, or data ready: fall through and read.
        }
        // Incomplete sequence (or empty buffer): pull more bytes.
        if (g_buf_len == sizeof g_buf) {
            g_buf_len = 0;   // overflow guard
        }
        ssize_t n_read = read(fd, g_buf + g_buf_len, sizeof g_buf - g_buf_len);
        if (n_read < 0) {
            if (errno == EINTR) {
                if (sc_tty_take_resize()) {
                    return resize;
                }
                continue;   // other signal: retry the read
            }
            return none;
        }
        if (n_read == 0) {
            return none;   // EOF
        }
        g_buf_len += (size_t)n_read;
    }
}

void sc_tty_input_reset(void) {
    g_buf_len = 0;
}

size_t sc_key_decode(const char *buf, size_t len, ScKey *out) {
    out->type = SC_KEY_NONE;
    out->codepoint = 0;
    out->bytes[0] = '\0';
    out->mods = SC_MOD_NONE;
    if (len == 0) {
        return 0;
    }

    unsigned char first = (unsigned char)buf[0];
    switch (first) {
        case 0x1b: return decode_escape(buf, len, out);
        case '\r': case '\n': out->type = SC_KEY_ENTER;     return 1;
        case '\t':            out->type = SC_KEY_TAB;       return 1;
        case 0x7f: case 0x08: out->type = SC_KEY_BACKSPACE; return 1;
        case 0x01:            out->type = SC_KEY_CTRL_A;    return 1;
        case 0x03:            out->type = SC_KEY_CTRL_C;    return 1;
        case 0x04:            out->type = SC_KEY_CTRL_D;    return 1;
        case 0x05:            out->type = SC_KEY_CTRL_E;    return 1;
        case 0x0b:            out->type = SC_KEY_CTRL_K;    return 1;
        case 0x15:            out->type = SC_KEY_CTRL_U;    return 1;
        case 0x17:            out->type = SC_KEY_CTRL_W;    return 1;
        default: break;
    }
    if (first >= 0x01 && first <= 0x1a) {
        // Any control byte not handled above (Tab/Enter/Backspace/Esc and the
        // named Ctrl-A/C/D/E/K/U/W) maps to Ctrl + its letter, so it can be a
        // shortcut. Reported as SC_KEY_CHAR + SC_MOD_CTRL; the line editor and
        // other text consumers ignore CHARs with a modifier, so it is not
        // typed.
        out->type = SC_KEY_CHAR;
        out->codepoint = (uint32_t)('a' - 1 + first);
        out->bytes[0] = (char)('a' - 1 + first);
        out->bytes[1] = '\0';
        out->mods = SC_MOD_CTRL;
        return 1;
    }
    if (first < 0x20) {
        out->type = SC_KEY_NONE;   // skip other control bytes
        return 1;
    }

    size_t seq_len = utf8_seq_len(first);
    if (seq_len == 0) {
        out->type = SC_KEY_NONE;   // invalid UTF-8 lead byte
        return 1;
    }
    if (len < seq_len) {
        return 0;                  // incomplete multi-byte sequence
    }
    make_char(out, buf, seq_len);
    return seq_len;
}

/**
 * Parses a CSI/SS3 escape sequence beginning at `buf` (which starts with
 * ESC). Returns bytes consumed, or 0 if the sequence is incomplete.
 */
static size_t decode_escape(const char *buf, size_t len, ScKey *out) {
    if (len < 2) {
        return 0;   // need at least ESC + 1
    }
    char kind = buf[1];
    if (kind != '[' && kind != 'O') {
        return decode_alt_prefix(buf, len, out);
    }
    if (len < 3) {
        return 0;
    }
    if (kind == 'O') {
        return decode_ss3(buf[2], out);
    }

    // CSI (ESC [ …): collect an optional numeric parameter.
    size_t i = 2;
    int param = 0;
    bool has_param = false;
    while (i < len && buf[i] >= '0' && buf[i] <= '9') {
        has_param = true;
        // Cap the accumulation so a long digit run can't overflow `int`
        // (signed overflow is UB). Any real CSI parameter is tiny; values
        // past the cap already fall through to SC_KEY_NONE.
        if (param < 1000000) { param = param * 10 + (buf[i] - '0'); }
        i++;
    }

    // Optional modifier parameter (e.g. ESC [ 6 ; 2 ~ = Shift+PageDown).
    int modifier = 0;
    if (i < len && buf[i] == ';') {
        i++;
        while (i < len && buf[i] >= '0' && buf[i] <= '9') {
            if (modifier < 1000000) {
                modifier = modifier * 10 + (buf[i] - '0');
            }
            i++;
        }
    }
    if (i >= len) {
        return 0;   // parameter not yet terminated
    }

    char final = buf[i];
    if (final == '~') {
        decode_csi_tilde(param, modifier, out);
        return i + 1;
    }
    if (has_param || !decode_csi_letter(final, out)) {
        out->type = SC_KEY_NONE;   // unrecognized CSI
    }
    return i + 1;
}

/** ESC + a normal byte: Alt/Meta + that key (decode the rest, flag ALT). */
static size_t decode_alt_prefix(const char *buf, size_t len, ScKey *out) {
    // A lone ESC (nothing follows) is resolved to SC_KEY_ESC by the buffered
    // reader's select() timeout, not here.
    ScKey sub;
    size_t used = sc_key_decode(buf + 1, len - 1, &sub);
    if (used == 0) {
        return 0;   // incomplete: wait for more bytes
    }
    *out = sub;
    out->mods |= SC_MOD_ALT;
    return used + 1;
}

/** SS3 (ESC O x): Home/End and F1-F4 on some terminals. */
static size_t decode_ss3(char final, ScKey *out) {
    switch (final) {
        case 'H': out->type = SC_KEY_HOME; return 3;
        case 'F': out->type = SC_KEY_END;  return 3;
        case 'P': out->type = SC_KEY_F1;   return 3;
        case 'Q': out->type = SC_KEY_F2;   return 3;
        case 'R': out->type = SC_KEY_F3;   return 3;
        case 'S': out->type = SC_KEY_F4;   return 3;
        default:  out->type = SC_KEY_ESC;  return 1;
    }
}

/** CSI `~` sequences keyed by the numeric parameter (ESC [ <param> ~). */
static void decode_csi_tilde(int param, int modifier, ScKey *out) {
    switch (param) {
        case 1: case 7: out->type = SC_KEY_HOME;   break;
        case 3:         out->type = SC_KEY_DELETE; break;
        case 4: case 8: out->type = SC_KEY_END;    break;
        case 5:
            out->type = (modifier == 2) ? SC_KEY_SHIFT_PAGEUP
                                        : SC_KEY_PAGEUP;
            break;
        case 6:
            out->type = (modifier == 2) ? SC_KEY_SHIFT_PAGEDOWN
                                        : SC_KEY_PAGEDOWN;
            break;
        case 11: out->type = SC_KEY_F1;  break;
        case 12: out->type = SC_KEY_F2;  break;
        case 13: out->type = SC_KEY_F3;  break;
        case 14: out->type = SC_KEY_F4;  break;
        case 15: out->type = SC_KEY_F5;  break;
        case 17: out->type = SC_KEY_F6;  break;
        case 18: out->type = SC_KEY_F7;  break;
        case 19: out->type = SC_KEY_F8;  break;
        case 20: out->type = SC_KEY_F9;  break;
        case 21: out->type = SC_KEY_F10; break;
        case 23: out->type = SC_KEY_F11; break;
        case 24: out->type = SC_KEY_F12; break;
        default:        out->type = SC_KEY_NONE;   break;
    }
}

/** Parameterless CSI letters (arrows, Home/End, Shift-Tab). */
static bool decode_csi_letter(char final, ScKey *out) {
    switch (final) {
        case 'A': out->type = SC_KEY_UP;      return true;
        case 'B': out->type = SC_KEY_DOWN;    return true;
        case 'C': out->type = SC_KEY_RIGHT;   return true;
        case 'D': out->type = SC_KEY_LEFT;    return true;
        case 'H': out->type = SC_KEY_HOME;    return true;
        case 'F': out->type = SC_KEY_END;     return true;
        case 'Z': out->type = SC_KEY_BACKTAB; return true;
        default:  return false;
    }
}

/** Fills `out` as a printable char from a complete UTF-8 sequence. */
static void make_char(ScKey *out, const char *bytes, size_t seq_len) {
    const unsigned char *b = (const unsigned char *)bytes;
    uint32_t codepoint = 0;
    switch (seq_len) {
        case 1:
            codepoint = b[0];
            break;
        case 2:
            codepoint = ((uint32_t)(b[0] & 0x1F) << 6) | (b[1] & 0x3F);
            break;
        case 3:
            codepoint = ((uint32_t)(b[0] & 0x0F) << 12)
                      | ((uint32_t)(b[1] & 0x3F) << 6) | (b[2] & 0x3F);
            break;
        default:
            codepoint = ((uint32_t)(b[0] & 0x07) << 18)
                      | ((uint32_t)(b[1] & 0x3F) << 12)
                      | ((uint32_t)(b[2] & 0x3F) << 6) | (b[3] & 0x3F);
            break;
    }
    out->type = SC_KEY_CHAR;
    out->codepoint = codepoint;
    memcpy(out->bytes, bytes, seq_len);
    out->bytes[seq_len] = '\0';
}

/** Number of bytes a UTF-8 lead byte announces (1-4); 0 = invalid lead. */
static size_t utf8_seq_len(unsigned char lead) {
    if ((lead & 0x80) == 0x00) {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0) {
        return 2;
    }
    if ((lead & 0xF0) == 0xE0) {
        return 3;
    }
    if ((lead & 0xF8) == 0xF0) {
        return 4;
    }
    return 0;
}
