#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── sc_strip_ansi ───────────────────────────────────────────────────────── */

char *sc_strip_ansi(const char *str) {
    if (!str) return strdup("");
    size_t len = strlen(str);
    char  *out = malloc(len + 1);
    if (!out) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; ) {
        if ((unsigned char)str[i] == '\033' && str[i + 1] == '[') {
            i += 2;
            /* skip parameter/intermediate bytes; stop at final byte 0x40–0x7E */
            while (str[i] && !((unsigned char)str[i] >= 0x40 &&
                                (unsigned char)str[i] <= 0x7E))
                i++;
            if (str[i]) i++;
        } else {
            out[j++] = str[i++];
        }
    }
    out[j] = '\0';
    return out;
}

/* ── sc_truncate ─────────────────────────────────────────────────────────── */

char *sc_truncate(const char *str, int max_cols, const char *ellipsis) {
    if (!str) return strdup("");

    int vis_w = (int)sc_utf8_vis_w(str, strlen(str));
    if (vis_w <= max_cols) return strdup(str);

    int ell_w = ellipsis ? (int)sc_utf8_vis_w(ellipsis, strlen(ellipsis)) : 0;
    int fit   = max_cols - ell_w;
    if (fit < 0) fit = 0;

    size_t trim_bytes = sc_utf8_trim_to_cols(str, fit);
    size_t ell_bytes  = ellipsis ? strlen(ellipsis) : 0;

    char *out = malloc(trim_bytes + ell_bytes + 1);
    if (!out) return NULL;
    memcpy(out, str, trim_bytes);
    if (ellipsis) memcpy(out + trim_bytes, ellipsis, ell_bytes);
    out[trim_bytes + ell_bytes] = '\0';
    return out;
}

/* ── sc_clear_line ───────────────────────────────────────────────────────── */

void sc_clear_line(void) {
    int tw = sc_term_width();
    fputc('\r', stdout);
    for (int i = 0; i < tw; i++) fputc(' ', stdout);
    fputc('\r', stdout);
    fflush(stdout);
}
