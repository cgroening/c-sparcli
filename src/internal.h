#ifndef SPARCLI_INTERNAL_H
#define SPARCLI_INTERNAL_H

#include "sparcli.h"
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

void sc_apply_colors(ScColor fg, ScColor bg);
void sc_apply_style (ScTextAttribute style);

static inline int sc_term_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    return 80;
}

/* Count visible terminal columns in the first byte_len bytes of a UTF-8
   string.  Assumes all code points occupy 1 column (correct for non-CJK). */
static inline size_t sc_utf8_vis_w(const char *s, size_t byte_len) {
    size_t cols = 0;
    const unsigned char *p   = (const unsigned char *)s;
    const unsigned char *end = p + byte_len;
    while (p < end) {
        unsigned char c = *p;
        if      ((c & 0x80) == 0x00) p += 1;
        else if ((c & 0xE0) == 0xC0) p += 2;
        else if ((c & 0xF0) == 0xE0) p += 3;
        else                          p += 4;
        cols++;
    }
    return cols;
}

/* Return the number of bytes of s that fit within max_cols visible columns. */
static inline size_t sc_utf8_trim_to_cols(const char *s, int max_cols) {
    const unsigned char *p = (const unsigned char *)s;
    int cols = 0;
    while (*p && cols < max_cols) {
        unsigned char c = *p;
        if      ((c & 0x80) == 0x00) p += 1;
        else if ((c & 0xE0) == 0xC0) p += 2;
        else if ((c & 0xF0) == 0xE0) p += 3;
        else                          p += 4;
        cols++;
    }
    return (size_t)(p - (const unsigned char *)s);
}

#endif /* SPARCLI_INTERNAL_H */
