#include "tty_internal.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>


static size_t decode_escape(const char *buf, size_t len, ScKey *out);
    static void make_char(ScKey *out, const char *bytes, size_t seq_len);
    static size_t utf8_seq_len(unsigned char lead);


/* Buffered, not-yet-decoded input bytes, persisted across read calls. */
static char g_buf[64];
static size_t g_buf_len = 0;


ScKey sc_tty_read_key(void) {
    const ScKey none   = { .type = SC_KEY_NONE,   .codepoint = 0, .bytes = {0} };
    const ScKey resize = { .type = SC_KEY_RESIZE, .codepoint = 0, .bytes = {0} };

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
        if (g_buf_len > 0 && (unsigned char)g_buf[0] == 0x1b) {
            struct pollfd pfd = { .fd = fd, .events = POLLIN };
            int poll_result = poll(&pfd, 1, 25 /* ms */);
            if (poll_result == 0) {
                memmove(g_buf, g_buf + 1, g_buf_len - 1);
                g_buf_len -= 1;
                ScKey esc = { .type = SC_KEY_ESC, .codepoint = 0, .bytes = {0} };
                return esc;
            }
            if (poll_result < 0 && errno != EINTR) {
                return none;
            }
            if (poll_result < 0 && sc_tty_take_resize()) {
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
        out->type = SC_KEY_ESC;   // ESC + unrelated byte: consume only the ESC
        return 1;
    }
    if (len < 3) {
        return 0;
    }

    // SS3 (ESC O x): function-key encodings for Home/End on some terminals.
    if (kind == 'O') {
        switch (buf[2]) {
            case 'H': out->type = SC_KEY_HOME; return 3;
            case 'F': out->type = SC_KEY_END;  return 3;
            default:  out->type = SC_KEY_ESC;  return 1;
        }
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
            if (modifier < 1000000) { modifier = modifier * 10 + (buf[i] - '0'); }
            i++;
        }
    }
    if (i >= len) {
        return 0;   // parameter not yet terminated
    }

    char final = buf[i];
    if (final == '~') {
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
            default:        out->type = SC_KEY_NONE;   break;
        }
        return i + 1;
    }
    if (!has_param) {
        switch (final) {
            case 'A': out->type = SC_KEY_UP;      return i + 1;
            case 'B': out->type = SC_KEY_DOWN;    return i + 1;
            case 'C': out->type = SC_KEY_RIGHT;   return i + 1;
            case 'D': out->type = SC_KEY_LEFT;    return i + 1;
            case 'H': out->type = SC_KEY_HOME;    return i + 1;
            case 'F': out->type = SC_KEY_END;     return i + 1;
            case 'Z': out->type = SC_KEY_BACKTAB; return i + 1;
            default:  break;
        }
    }
    out->type = SC_KEY_NONE;   // unrecognized CSI
    return i + 1;
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

/** Number of bytes a UTF-8 lead byte announces (1–4); 0 = invalid lead. */
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
