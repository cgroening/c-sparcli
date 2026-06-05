#include "serde_internal.h"

#include <stdlib.h>
#include <string.h>

/**
 * @file buf.c
 * @brief `ScSerdeBuf`: the growable output buffer shared by the format
 *        serializers. Doubling growth, overflow-checked, sticky-fail.
 */


/** Initial capacity for the first append (keeps small documents at one
 *  allocation). */
#define SERDE_BUF_MIN_CAPACITY 64


/** Ensures room for `extra` more bytes plus a NUL; sets `failed` on error. */
static bool ensure_capacity(ScSerdeBuf *buf, size_t extra) {
    if (buf->failed) {
        return false;
    }

    // Need len + extra + 1 (NUL); guard the additions against overflow.
    if (extra > SIZE_MAX - 1 || buf->len > SIZE_MAX - 1 - extra) {
        buf->failed = true;
        return false;
    }
    size_t needed = buf->len + extra + 1;
    if (needed <= buf->cap) {
        return true;
    }

    size_t new_cap = buf->cap ? buf->cap : SERDE_BUF_MIN_CAPACITY;
    while (new_cap < needed) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = needed;
            break;
        }
        new_cap *= 2;
    }

    char *grown = realloc(buf->data, new_cap);
    if (!grown) {
        buf->failed = true;
        return false;
    }
    buf->data = grown;
    buf->cap = new_cap;
    return true;
}


bool sc_serde_buf_append(ScSerdeBuf *buf, const char *bytes, size_t len) {
    if (len == 0) {
        return !buf->failed;
    }
    if (!bytes || !ensure_capacity(buf, len)) {
        return false;
    }
    memcpy(buf->data + buf->len, bytes, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return true;
}


bool sc_serde_buf_append_str(ScSerdeBuf *buf, const char *text) {
    if (!text) {
        return !buf->failed;
    }
    return sc_serde_buf_append(buf, text, strlen(text));
}


bool sc_serde_buf_append_char(ScSerdeBuf *buf, char byte) {
    return sc_serde_buf_append(buf, &byte, 1);
}


char *sc_serde_buf_finish(ScSerdeBuf *buf) {
    if (buf->failed) {
        sc_serde_buf_free(buf);
        return NULL;
    }

    // An empty buffer never allocated: hand back an owned empty string.
    if (!buf->data) {
        char *empty = malloc(1);
        if (empty) {
            empty[0] = '\0';
        }
        return empty;
    }

    char *result = buf->data;
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
    return result;
}


void sc_serde_buf_free(ScSerdeBuf *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
    buf->failed = false;
}
