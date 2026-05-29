#include "sparcli.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** ANSI Control Sequence Introducer (CSI) leading character. */
#define CSI_ESCAPE 0x1B

/** Start of the CSI parameter section: `[`. */
#define CSI_INTRODUCER '['

/** Lowest byte value treated as a CSI final byte. */
#define CSI_FINAL_MIN 0x40

/** Highest byte value treated as a CSI final byte. */
#define CSI_FINAL_MAX 0x7E


// Forward declarations indented to reflect call hierarchy
static size_t skip_csi_sequence(const char *str, size_t start);
    static bool is_csi_final_byte(unsigned char byte);



char *sc_strip_ansi(const char *str) {
    if (!str) { return strdup(""); }

    size_t input_length = strlen(str);
    char *output = malloc(input_length + 1);
    if (!output) { return NULL; }

    size_t write_position = 0;
    for (size_t read_position = 0; read_position < input_length; ) {
        unsigned char current_byte = (unsigned char)str[read_position];
        bool is_csi_start = current_byte == CSI_ESCAPE
            && str[read_position + 1] == CSI_INTRODUCER;
        if (is_csi_start) {
            read_position = skip_csi_sequence(str, read_position);
        } else {
            output[write_position++] = str[read_position++];
        }
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


/**
 * Returns the index of the byte just past the CSI sequence that starts at
 * `start` in `str`. `str[start]` is expected to be `ESC` and
 * `str[start + 1]` is expected to be `[`.
 */
static size_t skip_csi_sequence(const char *str, size_t start) {
    size_t position = start + 2;
    while (str[position] && !is_csi_final_byte((unsigned char)str[position])) {
        position++;
    }
    if (str[position]) { position++; }
    return position;
}

/** Returns `true` when `byte` is a valid CSI final byte (0x40–0x7E). */
static bool is_csi_final_byte(unsigned char byte) {
    return byte >= CSI_FINAL_MIN && byte <= CSI_FINAL_MAX;
}

