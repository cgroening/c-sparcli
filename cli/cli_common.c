/**
 * @file cli_common.c
 * Shared context, error reporting and output-capture helpers for the CLI.
 */
#include "cli_common.h"
#include "cli_parse.h"
#include "cli_stdin.h"

#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


ScCliCtx sc_cli_ctx_init(const char *prog) {
    const char *no_color = getenv("NO_COLOR");
    return (ScCliCtx){
        .markup     = true,
        .color      = (no_color == NULL || no_color[0] == '\0'),
        .allow_ansi = false,
        .prog       = prog,
    };
}

int sc_cli_error(const ScCliCtx *ctx, const char *fmt, ...) {
    fprintf(stderr, "%s: ", ctx->prog);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
    return SC_CLI_EXIT_ERROR;
}

bool sc_cli_common_opt(ScCliCtx *ctx, int opt) {
    if (opt == SC_CLI_OPT_NO_MARKUP) {
        ctx->markup = false;
        return true;
    }
    if (opt == SC_CLI_OPT_NO_COLOR) {
        ctx->color = false;
        return true;
    }
    if (opt == SC_CLI_OPT_ALLOW_ANSI) {
        ctx->allow_ansi = true;
        sc_set_allow_ansi(true);
        return true;
    }
    return false;
}

bool sc_cli_input_opt(ScCliCtx *ctx, int opt, ScCliInputArgs *args) {
    if (opt == SC_CLI_OPT_ACCENT) {
        ScColor accent = { 0 };
        if (!sc_cli_parse_color(optarg, &accent)) {
            return false;
        }
        /* The theme accent reaches every widget as its default accent. */
        sc_input_set_theme(&(ScInputTheme){ .accent = accent });
        return true;
    }
    if (opt == SC_CLI_OPT_HINT) {
        args->hint = optarg;
        return true;
    }
    if (opt == SC_CLI_OPT_NO_HINT) {
        args->hide_hint = true;
        return true;
    }
    return sc_cli_common_opt(ctx, opt);
}

int sc_cli_input_exit(ScInputStatus status) {
    static const int EXIT_CODES[] = {
        [SC_INPUT_OK]        = SC_CLI_EXIT_OK,
        [SC_INPUT_CANCELLED] = SC_CLI_EXIT_CANCELLED,
        [SC_INPUT_ERROR]     = SC_CLI_EXIT_ERROR,
    };
    return EXIT_CODES[status];
}

ScText *sc_cli_text(const ScCliCtx *ctx, const char *arg) {
    if (ctx->markup) {
        return sc_markup_parse(arg);
    }

    ScText *text = sc_text_new();
    if (text == NULL) {
        return NULL;
    }
    sc_text_append(text, arg, (ScTextStyle){ 0 });
    return text;
}

/* Joins `argc` words with single spaces into one heap string. */
static char *join_words(int argc, char **argv);

char *sc_cli_content(int argc, char **argv) {
    if (argc > 0) {
        return join_words(argc, argv);
    }

    /* No positional arguments: read stdin and strip one final newline. */
    char *data = sc_cli_read_source(NULL);
    if (data == NULL) {
        return NULL;
    }
    size_t len = strlen(data);
    if (len > 0 && data[len - 1] == '\n') {
        data[len - 1] = '\0';
    }
    return data;
}

static char *join_words(int argc, char **argv) {
    size_t total = 1;
    for (int i = 0; i < argc; i++) {
        total += strlen(argv[i]) + 1;
    }

    char *joined = malloc(total);
    if (joined == NULL) {
        return NULL;
    }

    size_t position = 0;
    for (int i = 0; i < argc; i++) {
        if (i > 0) {
            joined[position++] = ' ';
        }
        size_t length = strlen(argv[i]);
        memcpy(joined + position, argv[i], length);
        position += length;
    }
    joined[position] = '\0';
    return joined;
}

FILE *sc_cli_capture_begin(const ScCliCtx *ctx) {
    if (ctx->color) {
        return NULL;
    }

    FILE *capture = tmpfile();
    if (capture == NULL) {
        return NULL;
    }
    sc_output_set_stream(capture);
    return capture;
}

/* Reads the full (rewound) contents of `stream` into a heap buffer. */
static char *read_stream(FILE *stream);

int sc_cli_capture_end(FILE *capture) {
    if (capture == NULL) {
        return SC_CLI_EXIT_OK;
    }
    sc_output_set_stream(NULL);

    char *raw = read_stream(capture);
    fclose(capture);
    if (raw == NULL) {
        return SC_CLI_EXIT_ERROR;
    }

    char *stripped = sc_strip_ansi(raw);
    free(raw);
    if (stripped == NULL) {
        return SC_CLI_EXIT_ERROR;
    }

    fputs(stripped, stdout);
    free(stripped);
    return SC_CLI_EXIT_OK;
}

static char *read_stream(FILE *stream) {
    if (fflush(stream) != 0 || fseek(stream, 0, SEEK_END) != 0) {
        return NULL;
    }
    long size = ftell(stream);
    if (size < 0 || fseek(stream, 0, SEEK_SET) != 0) {
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        return NULL;
    }
    size_t read_count = fread(buffer, 1, (size_t)size, stream);
    buffer[read_count] = '\0';
    return buffer;
}
