/**
 * @file cli_stdin.c
 * File-or-stdin reading and line splitting for the CLI commands.
 */
#include "cli_stdin.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SC_CLI_READ_CHUNK = 4096 };

/**
 * Upper bound for file/stdin input (64 MiB). Rendering terminal output from
 * larger inputs is never useful, and the cap keeps untrusted input from
 * exhausting memory (the whole source is held in one buffer).
 */
enum { SC_CLI_MAX_INPUT_BYTES = 64 * 1024 * 1024 };

static char *read_all(FILE *stream);

char *sc_cli_read_source(const char *path) {
    bool from_stdin = (path == NULL || strcmp(path, "-") == 0);
    if (from_stdin) {
        return read_all(stdin);
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    char *data = read_all(file);
    fclose(file);
    return data;
}

char **sc_cli_split_lines(char *data, size_t *count) {
    /* Count lines first so the pointer array can be allocated in one go. */
    size_t line_count = 0;
    for (const char *cursor = data; *cursor != '\0'; cursor++) {
        if (*cursor == '\n') {
            line_count++;
        }
    }
    if (data[0] != '\0' && data[strlen(data) - 1] != '\n') {
        line_count++; /* last line without trailing newline */
    }

    char **lines = malloc((line_count > 0 ? line_count : 1)
                          * sizeof(*lines));
    if (lines == NULL) {
        return NULL;
    }

    /* Split in place: terminate each line at '\n' and strip a final '\r'.
       `filled` is the number of entries actually written, which is what
       the caller gets - so every reported line is always initialized. */
    size_t filled = 0;
    char  *line   = data;
    while (filled < line_count) {
        lines[filled] = line;
        filled++;

        char *newline = strchr(line, '\n');
        if (newline == NULL) {
            break;
        }
        *newline = '\0';
        line = newline + 1;
    }
    for (size_t i = 0; i < filled; i++) {
        size_t len = strlen(lines[i]);
        if (len > 0 && lines[i][len - 1] == '\r') {
            lines[i][len - 1] = '\0';
        }
    }

    *count = filled;
    return lines;
}

/* Reads a stream to EOF into a growing heap buffer. */
static char *read_all(FILE *stream) {
    size_t capacity = SC_CLI_READ_CHUNK;
    size_t size     = 0;
    char  *buffer   = malloc(capacity);
    if (buffer == NULL) {
        return NULL;
    }

    while (true) {
        if (size + SC_CLI_READ_CHUNK > SC_CLI_MAX_INPUT_BYTES) {
            free(buffer);   // oversized input: treat like a read failure
            return NULL;
        }
        if (size + SC_CLI_READ_CHUNK + 1 > capacity) {
            size_t new_capacity = capacity * 2;
            char  *grown        = realloc(buffer, new_capacity);
            if (grown == NULL) {
                free(buffer);
                return NULL;
            }
            buffer   = grown;
            capacity = new_capacity;
        }

        size_t read_count = fread(buffer + size, 1, SC_CLI_READ_CHUNK,
                                  stream);
        size += read_count;
        if (read_count < SC_CLI_READ_CHUNK) {
            if (ferror(stream)) {
                free(buffer);
                return NULL;
            }
            break;
        }
    }

    buffer[size] = '\0';
    return buffer;
}
