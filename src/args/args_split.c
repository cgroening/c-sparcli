#include "sparcli.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/**
 * @file args_split.c
 * @brief Quote-aware command-line tokenizer for REPL loops.
 *
 * Splits one input line into an argv vector that `sc_args_parse` accepts
 * (argv[0] = program name, parsing starts at index 1). Pure string
 * processing: no sanitization here - `sc_args_parse` sanitizes every token
 * itself - and no terminal interaction.
 */


/** Initial capacity of the argv array. */
#define SPLIT_ARGV_INITIAL_CAPACITY 8

/** Initial capacity of the per-token byte buffer. */
#define SPLIT_TOKEN_INITIAL_CAPACITY 32


/** Growable byte buffer for the token currently being assembled. */
typedef struct TokenBuffer {
    char  *bytes;
    size_t length;
    size_t capacity;
} TokenBuffer;

/** Growable argv array under construction. */
typedef struct ArgvBuilder {
    char **items;
    size_t count;
    size_t capacity;
} ArgvBuilder;


// Forward declarations indented to reflect call hierarchy
static bool split_line(
    ArgvBuilder *argv, const char *line, char *err, size_t err_size
);
    static bool read_token(
        ArgvBuilder *argv, const char **cursor, char *err, size_t err_size
    );
        static bool read_single_quoted(
            TokenBuffer *token, const char **cursor, char *err, size_t err_size
        );
        static bool read_double_quoted(
            TokenBuffer *token, const char **cursor, char *err, size_t err_size
        );
static bool argv_push(ArgvBuilder *argv, char *item);
static void argv_discard(ArgvBuilder *argv);
static bool token_append(TokenBuffer *token, char byte);
static char *token_take(TokenBuffer *token);
static bool is_split_space(char byte);
static void write_error(char *err, size_t err_size, const char *message);


char **sc_args_split(
    const char *prog, const char *line, int *argc,
    char *err, size_t err_size
) {
    if (argc) { *argc = 0; }
    write_error(err, err_size, "");

    ArgvBuilder argv = { 0 };
    char *prog_copy = strdup(prog ? prog : "");
    if (!prog_copy || !argv_push(&argv, prog_copy)) {
        free(prog_copy);
        argv_discard(&argv);
        write_error(err, err_size, "out of memory");
        return NULL;
    }

    if (!split_line(&argv, line ? line : "", err, err_size)) {
        argv_discard(&argv);
        return NULL;
    }

    // NULL terminator so the result can be freed without knowing the count
    if (!argv_push(&argv, NULL)) {
        argv_discard(&argv);
        write_error(err, err_size, "out of memory");
        return NULL;
    }

    if (argc) { *argc = (int)(argv.count - 1); }
    return argv.items;
}

void sc_args_split_free(char **argv) {
    if (!argv) { return; }
    for (size_t i = 0; argv[i]; i++) {
        free(argv[i]);
    }
    free(argv);
}


/* ── Internals ──────────────────────────────────────────────────────────── */

/** Tokenizes the whole line, appending every token to `argv`. */
static bool split_line(
    ArgvBuilder *argv, const char *line, char *err, size_t err_size
) {
    const char *cursor = line;
    for (;;) {
        while (is_split_space(*cursor)) { cursor++; }
        if (*cursor == '\0') { return true; }
        if (!read_token(argv, &cursor, err, err_size)) { return false; }
    }
}

/**
 * Reads one token starting at `*cursor` (not whitespace, not NUL) and pushes
 * it onto `argv`. A token may concatenate bare, single-quoted and
 * double-quoted runs (`a"b c"d` is one token `ab cd`).
 */
static bool read_token(
    ArgvBuilder *argv, const char **cursor, char *err, size_t err_size
) {
    TokenBuffer token = { 0 };
    const char *at = *cursor;

    while (*at != '\0' && !is_split_space(*at)) {
        bool ok = true;
        if (*at == '\'') {
            at++;
            ok = read_single_quoted(&token, &at, err, err_size);
        } else if (*at == '"') {
            at++;
            ok = read_double_quoted(&token, &at, err, err_size);
        } else if (*at == '\\') {
            at++;
            if (*at == '\0') {
                write_error(err, err_size, "trailing backslash");
                ok = false;
            } else {
                ok = token_append(&token, *at++);
            }
        } else {
            ok = token_append(&token, *at++);
        }

        if (!ok) {
            free(token.bytes);
            return false;
        }
    }

    char *text = token_take(&token);
    if (!text || !argv_push(argv, text)) {
        free(text);
        write_error(err, err_size, "out of memory");
        return false;
    }
    *cursor = at;
    return true;
}

/** Single quotes: every byte is literal until the closing quote. */
static bool read_single_quoted(
    TokenBuffer *token, const char **cursor, char *err, size_t err_size
) {
    const char *at = *cursor;
    while (*at != '\'') {
        if (*at == '\0') {
            write_error(err, err_size, "unterminated quote");
            return false;
        }
        if (!token_append(token, *at++)) {
            write_error(err, err_size, "out of memory");
            return false;
        }
    }
    *cursor = at + 1;   // skip the closing quote
    return true;
}

/** Double quotes: backslash escapes the next byte; ends at the closing quote. */
static bool read_double_quoted(
    TokenBuffer *token, const char **cursor, char *err, size_t err_size
) {
    const char *at = *cursor;
    while (*at != '"') {
        if (*at == '\0') {
            write_error(err, err_size, "unterminated quote");
            return false;
        }
        if (*at == '\\' && at[1] != '\0') {
            at++;
        }
        if (!token_append(token, *at++)) {
            write_error(err, err_size, "out of memory");
            return false;
        }
    }
    *cursor = at + 1;   // skip the closing quote
    return true;
}

/** Appends `item` to the argv array, growing it as needed. */
static bool argv_push(ArgvBuilder *argv, char *item) {
    if (argv->count == argv->capacity) {
        size_t new_capacity = argv->capacity
            ? argv->capacity * 2
            : SPLIT_ARGV_INITIAL_CAPACITY;
        char **grown = realloc(argv->items, new_capacity * sizeof(char *));
        if (!grown) { return false; }
        argv->items = grown;
        argv->capacity = new_capacity;
    }
    argv->items[argv->count++] = item;
    return true;
}

/** Frees a partially built argv (entries + array). */
static void argv_discard(ArgvBuilder *argv) {
    for (size_t i = 0; i < argv->count; i++) {
        free(argv->items[i]);
    }
    free(argv->items);
    argv->items = NULL;
    argv->count = 0;
    argv->capacity = 0;
}

/** Appends one byte to the token buffer, growing it as needed. */
static bool token_append(TokenBuffer *token, char byte) {
    if (token->length + 1 >= token->capacity) {
        size_t new_capacity = token->capacity
            ? token->capacity * 2
            : SPLIT_TOKEN_INITIAL_CAPACITY;
        char *grown = realloc(token->bytes, new_capacity);
        if (!grown) { return false; }
        token->bytes = grown;
        token->capacity = new_capacity;
    }
    token->bytes[token->length++] = byte;
    token->bytes[token->length] = '\0';
    return true;
}

/** Finishes the token: returns its heap string (empty tokens included). */
static char *token_take(TokenBuffer *token) {
    if (!token->bytes) {
        return strdup("");
    }
    return token->bytes;
}

/** The token separators: space, tab and (unquoted) line breaks. */
static bool is_split_space(char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r';
}

/** Writes `message` into the caller's error buffer, if one was given. */
static void write_error(char *err, size_t err_size, const char *message) {
    if (err && err_size > 0) {
        snprintf(err, err_size, "%s", message);
    }
}
