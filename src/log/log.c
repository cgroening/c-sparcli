#include "sparcli.h"
#include "core/sanitize_internal.h"
#include "core/text_internal.h"
#include "platform/sc_compat.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


/** Maximum number of file sinks on the global logger. */
#define LOG_MAX_GLOBAL_FILE_SINKS 4

/** Initial sink capacity of a handle-based logger. */
#define LOG_INITIAL_SINK_CAPACITY 2

/** Default level of the global logger's stderr sink. */
#define LOG_GLOBAL_DEFAULT_LEVEL SC_LOG_INFO

/** Buffer size for the timestamp prefix. */
#define LOG_TIMESTAMP_BUFFER 24

/** Buffer size for the `file:line` suffix. */
#define LOG_SOURCE_BUFFER 256


/** Per-level tag label and style (equal-width labels keep columns aligned). */
static const struct {
    const char     *label;
    ScColor         color;
    ScTextAttribute attr;
} LEVEL_PRESETS[] = {
    [SC_LOG_DEBUG] = { "DEBUG", SC_ANSI_COLOR_MAGENTA, SC_TEXT_ATTR_DIM  },
    [SC_LOG_INFO]  = { "INFO ", SC_ANSI_COLOR_CYAN,    SC_TEXT_ATTR_BOLD },
    [SC_LOG_WARN]  = { "WARN ", SC_ANSI_COLOR_YELLOW,  SC_TEXT_ATTR_BOLD },
    [SC_LOG_ERROR] = { "ERROR", SC_ANSI_COLOR_RED,     SC_TEXT_ATTR_BOLD },
};

/** Span styles shared by every record. */
static const ScTextStyle DIM_STYLE = {
    SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle PLAIN_STYLE = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};


/** One output target of a logger. */
typedef struct LogSink {
    FILE      *stream;       /**< Target stream. */
    ScLogLevel min_level;    /**< Minimum level this sink emits. */
    bool       plain;        /**< Strip ANSI before writing (file sinks). */
    bool       owns_stream;  /**< `fclose` the stream when freed. */
} LogSink;

/** Logger instance (opaque to callers). */
struct ScLogger {
    ScLoggerOpts opts;           /**< Format options + overall gate. */
    LogSink     *sinks;          /**< Output targets. */
    size_t       sink_count;     /**< Number of sinks. */
    size_t       sink_capacity;  /**< Allocated sink slots. */
};


/*
 * Global logger state. The terminal level and the file-sink count are
 * atomics so concurrent log calls are race-free; the opts struct and the
 * sink array entries are set-once configuration (documented: configure
 * before spawning threads).
 */
static atomic_int g_terminal_level = LOG_GLOBAL_DEFAULT_LEVEL;
static ScLoggerOpts g_global_opts;
static LogSink g_file_sinks[LOG_MAX_GLOBAL_FILE_SINKS];
static atomic_size_t g_file_sink_count;


// Forward declarations indented to reflect call hierarchy
static void emit_record(
    ScLoggerOpts opts, const LogSink *sinks, size_t sink_count,
    ScLogLevel level, const char *file, int line,
    const char *format, va_list args
)
    SC_ATTR_FORMAT(7, 0);
    static char *format_message(const char *format, va_list args)
        SC_ATTR_FORMAT(1, 0);
    static ScText *build_record(
        ScLoggerOpts opts, ScLogLevel level,
        const char *file, int line, const char *message
    );
        static void append_message(
            ScText *record, ScLoggerOpts opts, const char *message
        );
    static char *render_to_string(const ScText *record);
    static void write_to_sink(
        const LogSink *sink, ScLogLevel level, const char *colored
    );
static bool add_sink(ScLogger *logger, LogSink sink);


/* ── Handle-based loggers ───────────────────────────────────────────────── */

ScLogger *sc_logger_new(ScLoggerOpts opts) {
    ScLogger *logger = calloc(1, sizeof(ScLogger));
    if (!logger) { return NULL; }
    logger->opts = opts;
    return logger;
}

void sc_logger_add_terminal(
    ScLogger *logger, FILE *stream, ScLogLevel min_level
) {
    if (!logger || !stream) { return; }
    add_sink(logger, (LogSink){
        .stream = stream,
        .min_level = min_level,
        .plain = false,
        .owns_stream = false,
    });
}

bool sc_logger_add_file(
    ScLogger *logger, const char *path, ScLogLevel min_level
) {
    if (!logger || !path) { return false; }

    FILE *stream = fopen(path, "a");
    if (!stream) { return false; }

    bool added = add_sink(logger, (LogSink){
        .stream = stream,
        .min_level = min_level,
        .plain = true,
        .owns_stream = true,
    });
    if (!added) {
        fclose(stream);
        return false;
    }
    return true;
}

void sc_logger_set_level(ScLogger *logger, ScLogLevel level) {
    if (!logger) { return; }
    logger->opts.level = level;
}

void sc_logger_log(
    ScLogger *logger, ScLogLevel level, const char *file, int line,
    const char *format, ...
) {
    if (!logger) { return; }

    va_list args;
    va_start(args, format);
    emit_record(
        logger->opts, logger->sinks, logger->sink_count,
        level, file, line, format, args
    );
    va_end(args);
}

void sc_logger_free(ScLogger *logger) {
    if (!logger) { return; }
    for (size_t i = 0; i < logger->sink_count; i++) {
        if (logger->sinks[i].owns_stream) {
            fclose(logger->sinks[i].stream);
        }
    }
    free(logger->sinks);
    free(logger);
}


/* ── Global logger ──────────────────────────────────────────────────────── */

void sc_log_set_level(ScLogLevel level) {
    atomic_store_explicit(&g_terminal_level, (int)level, memory_order_relaxed);
}

ScLogLevel sc_log_level(void) {
    return (ScLogLevel)atomic_load_explicit(
        &g_terminal_level, memory_order_relaxed
    );
}

void sc_log_set_opts(ScLoggerOpts opts) {
    // The global gate is per sink; the opts level field is not used
    opts.level = SC_LOG_DEBUG;
    g_global_opts = opts;
}

bool sc_log_add_file(const char *path, ScLogLevel min_level) {
    if (!path) { return false; }

    size_t count = atomic_load_explicit(
        &g_file_sink_count, memory_order_acquire
    );
    if (count >= LOG_MAX_GLOBAL_FILE_SINKS) { return false; }

    FILE *stream = fopen(path, "a");
    if (!stream) { return false; }

    g_file_sinks[count] = (LogSink){
        .stream = stream,
        .min_level = min_level,
        .plain = true,
        .owns_stream = true,
    };
    // Publish the new sink only after it is fully initialized
    atomic_store_explicit(
        &g_file_sink_count, count + 1, memory_order_release
    );
    return true;
}

void sc_log_reset(void) {
    // Unpublish the sinks first so concurrent log calls stop using them
    size_t count = atomic_load_explicit(
        &g_file_sink_count, memory_order_acquire
    );
    atomic_store_explicit(&g_file_sink_count, 0, memory_order_release);

    for (size_t i = 0; i < count; i++) {
        if (g_file_sinks[i].owns_stream) {
            fclose(g_file_sinks[i].stream);
        }
    }
    g_global_opts = (ScLoggerOpts){ 0 };
    atomic_store_explicit(
        &g_terminal_level, LOG_GLOBAL_DEFAULT_LEVEL, memory_order_relaxed
    );
}

void sc_log_log(
    ScLogLevel level, const char *file, int line, const char *format, ...
) {
    // Assemble the sink list: built-in stderr terminal + published files
    LogSink sinks[LOG_MAX_GLOBAL_FILE_SINKS + 1];
    size_t sink_count = 0;

    int stderr_fd = fileno(stderr);
    sinks[sink_count++] = (LogSink){
        .stream = stderr,
        .min_level = sc_log_level(),
        // Colors only when stderr is really a terminal
        .plain = stderr_fd < 0 || !isatty(stderr_fd),
        .owns_stream = false,
    };

    size_t file_count = atomic_load_explicit(
        &g_file_sink_count, memory_order_acquire
    );
    for (size_t i = 0; i < file_count; i++) {
        sinks[sink_count++] = g_file_sinks[i];
    }

    ScLoggerOpts opts = g_global_opts;
    opts.level = SC_LOG_DEBUG;   // gating happens per sink

    va_list args;
    va_start(args, format);
    emit_record(opts, sinks, sink_count, level, file, line, format, args);
    va_end(args);
}


/* ── Record pipeline ────────────────────────────────────────────────────── */

/**
 * Renders one record and writes it to every sink that accepts `level`.
 * The record is rendered once (colored); file sinks receive the
 * ANSI-stripped copy.
 */
static void emit_record(
    ScLoggerOpts opts, const LogSink *sinks, size_t sink_count,
    ScLogLevel level, const char *file, int line,
    const char *format, va_list args
) {
    // Validate the level (it indexes LEVEL_PRESETS) and the overall gate
    if (level < SC_LOG_DEBUG || level > SC_LOG_ERROR) { return; }
    if (level < opts.level) { return; }

    // Early out when no sink would emit this record
    bool wanted = false;
    for (size_t i = 0; i < sink_count; i++) {
        if (sinks[i].min_level != SC_LOG_OFF
            && level >= sinks[i].min_level) {
            wanted = true;
            break;
        }
    }
    if (!wanted) { return; }

    char *message = format_message(format, args);
    if (!message) { return; }

    ScText *record = build_record(opts, level, file, line, message);
    free(message);
    if (!record) { return; }

    char *colored = render_to_string(record);
    sc_text_free(record);
    if (!colored) { return; }

    for (size_t i = 0; i < sink_count; i++) {
        write_to_sink(&sinks[i], level, colored);
    }
    free(colored);
}

/** printf-formats the message into a heap string (caller frees). */
static char *format_message(const char *format, va_list args) {
    if (!format) { return strdup(""); }

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (needed < 0) { return NULL; }

    char *message = malloc((size_t)needed + 1);
    if (!message) { return NULL; }
    vsnprintf(message, (size_t)needed + 1, format, args);
    return message;
}

/**
 * Builds the record as rich text:
 * `[HH:MM:SS] LEVEL message  file:line` (timestamp + source dim, level
 * tag colored, message plain or markup-parsed).
 */
static ScText *build_record(
    ScLoggerOpts opts, ScLogLevel level,
    const char *file, int line, const char *message
) {
    ScText *record = sc_text_new();
    if (!record) { return NULL; }

    if (!opts.hide_timestamps) {
        char stamp[LOG_TIMESTAMP_BUFFER];
        time_t now = time(NULL);
        struct tm time_parts;
        localtime_r(&now, &time_parts);
        strftime(stamp, sizeof stamp, "[%H:%M:%S] ", &time_parts);
        sc_text_append_raw(record, stamp, DIM_STYLE);
    }

    ScTextStyle level_style = {
        LEVEL_PRESETS[level].attr,
        LEVEL_PRESETS[level].color,
        SC_ANSI_COLOR_NONE,
    };
    sc_text_append_raw(record, LEVEL_PRESETS[level].label, level_style);
    sc_text_append_raw(record, " ", PLAIN_STYLE);

    append_message(record, opts, message);

    if (opts.show_source && file) {
        // Only the basename keeps the suffix short
        const char *base = strrchr(file, '/');
        base = base ? base + 1 : file;
        char location[LOG_SOURCE_BUFFER];
        snprintf(location, sizeof location, "  %s:%d", base, line);
        sc_text_append_raw(record, location, DIM_STYLE);
    }

    return record;
}

/**
 * Appends the formatted message to the record: markup-parsed when the
 * logger asks for it, otherwise sanitized plain text. Either path crosses
 * the ANSI-injection trust boundary exactly once.
 */
static void append_message(
    ScText *record, ScLoggerOpts opts, const char *message
) {
    if (opts.markup) {
        sc_markup_append_opts(
            record, message, (ScMarkupOpts){ .ansi = opts.ansi }
        );
        return;
    }
    char *clean = sc_sanitize_copy_mode(message, opts.ansi);
    if (clean) {
        sc_text_append_raw(record, clean, PLAIN_STYLE);
        free(clean);
    }
}

/**
 * Renders the record into a heap string with ANSI colors + trailing
 * newline (caller frees). Uses a memory stream as the thread-local
 * output target, so concurrent loggers never interfere.
 */
static char *render_to_string(const ScText *record) {
    char *buffer = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buffer, &size);
    if (!stream) { return NULL; }

    FILE *previous = sc_output_stream();
    sc_output_set_stream(stream);
    sc_print_text(record);
    fputc('\n', stream);
    sc_output_set_stream(previous);

    fclose(stream);
    return buffer;
}

/**
 * Writes one rendered record to a sink (single `fputs`, so concurrent
 * records never interleave mid-line). File sinks get the ANSI-stripped
 * copy.
 */
static void write_to_sink(
    const LogSink *sink, ScLogLevel level, const char *colored
) {
    if (sink->min_level == SC_LOG_OFF || level < sink->min_level) {
        return;
    }

    if (sink->plain) {
        char *plain = sc_strip_ansi(colored);
        if (plain) {
            fputs(plain, sink->stream);
            free(plain);
        }
    } else {
        fputs(colored, sink->stream);
    }
    fflush(sink->stream);
}

/** Appends a sink to a handle-based logger (growing the array). */
static bool add_sink(ScLogger *logger, LogSink sink) {
    if (logger->sink_count == logger->sink_capacity) {
        size_t new_capacity = logger->sink_capacity
            ? logger->sink_capacity * 2
            : LOG_INITIAL_SINK_CAPACITY;
        LogSink *grown = realloc(
            logger->sinks, new_capacity * sizeof(LogSink)
        );
        if (!grown) { return false; }
        logger->sinks = grown;
        logger->sink_capacity = new_capacity;
    }
    logger->sinks[logger->sink_count++] = sink;
    return true;
}
