#include "sparcli.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** ANSI escape character introducing every escape sequence. */
#define ANSI_ESCAPE 0x1B


char *sc_strip_ansi(const char *str) {
    if (!str) { return strdup(""); }

    size_t input_length = strlen(str);
    char *output = malloc(input_length + 1);
    if (!output) { return NULL; }

    size_t write_position = 0;
    for (size_t read_position = 0; read_position < input_length; ) {
        unsigned char current_byte = (unsigned char)str[read_position];
        if (current_byte == ANSI_ESCAPE) {
            // Drop the whole sequence (CSI/OSC/DCS/two-char ESC); a lone
            // or malformed ESC is dropped byte by byte.
            const char *sequence_end = sc_ansi_skip_seq(str + read_position);
            size_t sequence_length =
                (size_t)(sequence_end - (str + read_position));
            read_position += sequence_length > 0 ? sequence_length : 1;
            continue;
        }
        output[write_position++] = str[read_position++];
    }
    output[write_position] = '\0';
    return output;
}

char *sc_truncate(const char *str, int max_cols, const char *ellipsis) {
    if (!str) { return strdup(""); }

    int visible_width = (int)sc_utf8_string_length(str, strlen(str));
    if (visible_width <= max_cols) { return strdup(str); }

    int ellipsis_width = ellipsis
        ? (int)sc_utf8_string_length(ellipsis, strlen(ellipsis)) : 0;
    int fit_columns = max_cols - ellipsis_width;
    if (fit_columns < 0) { fit_columns = 0; }

    size_t kept_bytes = sc_utf8_trim_to_cols(str, fit_columns);
    size_t ellipsis_bytes = ellipsis ? strlen(ellipsis) : 0;

    char *output = malloc(kept_bytes + ellipsis_bytes + 1);
    if (!output) { return NULL; }

    memcpy(output, str, kept_bytes);
    if (ellipsis) {
        memcpy(output + kept_bytes, ellipsis, ellipsis_bytes);
    }
    output[kept_bytes + ellipsis_bytes] = '\0';
    return output;
}

void sc_clear_line(void) {
    int terminal_width = sc_terminal_width();
    fputc('\r', sc_output_stream());
    sc_print_spaces(terminal_width);
    fputc('\r', sc_output_stream());
    fflush(sc_output_stream());
}

void sc_terminal_size(int *width, int *height) {
    if (width)  { *width = sc_terminal_width(); }
    if (height) { *height = sc_terminal_height(); }
}

int sc_term_width(void)  { return sc_terminal_width(); }
int sc_term_height(void) { return sc_terminal_height(); }
