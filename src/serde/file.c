#include "serde_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file file.c
 * @brief Shared file read/write helpers behind the per-format `*_parse_file`
 *        and `*_write_file` convenience functions.
 */


/** Fills `err` with `message: path` at no particular line. */
static void set_file_error(
    ScParseError *err, const char *message, const char *path
) {
    if (!err) {
        return;
    }
    *err = (ScParseError){ 0 };
    snprintf(err->message, sizeof err->message, "%s: %s", message, path);
}


char *sc_serde_read_file(const char *path, size_t *out_len, ScParseError *err) {
    if (out_len) {
        *out_len = 0;
    }
    if (!path) {
        set_file_error(err, "no file path", "(null)");
        return NULL;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        set_file_error(err, "cannot open file", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_file_error(err, "cannot read file", path);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_file_error(err, "cannot read file", path);
        return NULL;
    }

    size_t length = (size_t)size;
    char *buffer = malloc(length + 1);
    if (!buffer) {
        fclose(file);
        set_file_error(err, "out of memory reading file", path);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, length, file);
    fclose(file);
    buffer[read_bytes] = '\0'; // read_bytes <= length (short read tolerated)
    if (out_len) {
        *out_len = read_bytes;
    }
    return buffer;
}


bool sc_serde_write_file(const char *path, const char *data) {
    if (!path || !data) {
        return false;
    }
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    size_t length = strlen(data);
    size_t wrote = fwrite(data, 1, length, file);
    bool ok = (wrote == length);
    if (fclose(file) != 0) {
        ok = false;
    }
    return ok;
}
