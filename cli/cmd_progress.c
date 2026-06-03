/**
 * @file cmd_progress.c
 * Streaming subcommands: `spin` (spinner around a child command) and
 * `progress` (progress bar fed by stdin values).
 */
#include "cli_commands.h"
#include "cli_common.h"
#include "cli_parse.h"

#include "sparcli.h"

#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum {
    SC_CLI_SPIN_TICK_MS    = 80,
    SC_CLI_SIGNAL_EXIT_BASE = 128,
    SC_CLI_LINE_BUFFER      = 256,
    SC_CLI_MS_PER_SECOND    = 1000,
    SC_CLI_NS_PER_MS        = 1000000,
};

static void sleep_ms(int milliseconds);


/* ── spin ───────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli spin`. */
typedef struct SpinArgs {
    ScSpinnerOpts opts;
    const char   *title;
} SpinArgs;

static int run_spin_command(ScCliCtx *ctx, const SpinArgs *args,
                            char **command);
    static int run_child(char **command);
    static int wait_with_spinner(pid_t pid, ScSpinner *spinner);
    static int exit_code_from_status(int status);
static int run_spin_stdin(const SpinArgs *args);

static const char SPIN_USAGE[] =
    "Usage: sparcli spin [options] -- COMMAND [ARGS...]\n"
    "       ... | sparcli spin [options]\n"
    "\n"
    "Show an animated spinner while COMMAND runs, then a success/failure\n"
    "mark. The spinner is drawn on the terminal (/dev/tty); COMMAND's\n"
    "output passes through untouched and its exit code is propagated.\n"
    "Without a COMMAND, the spinner runs while stdin is consumed.\n"
    "\n"
    "Options:\n"
    "  --title TEXT               Label next to the spinner\n"
    "  --spinner STYLE            braille|pipe|dots|arrow\n"
    "  --color COLOR              Spinner color\n"
    SC_CLI_COMMON_USAGE
    "\n"
    "--style elements: label\n";

int sc_cli_cmd_spin(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_TITLE = SC_CLI_OPT_CMD_BASE,
        OPT_SPINNER,
        OPT_COLOR,
    };
    static const struct option longopts[] = {
        { "title",   required_argument, NULL, OPT_TITLE },
        { "spinner", required_argument, NULL, OPT_SPINNER },
        { "color",   required_argument, NULL, OPT_COLOR },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    SpinArgs       args   = { .title = "Working..." };
    ScCliStyleArgs styles = { 0 };
    int            opt    = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_TITLE:
            args.title = optarg;
            break;
        case OPT_SPINNER:
            if (!sc_cli_parse_spinner(optarg, &args.opts.type)) {
                return sc_cli_error(ctx, "invalid spinner '%s'", optarg);
            }
            break;
        case OPT_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.color)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_STYLE:
            sc_cli_style_collect(&styles, optarg);
            break;
        case SC_CLI_OPT_HELP:
            fputs(SPIN_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    const ScCliStyleSlot slots[] = {
        { "label", &args.opts.label_style },
    };
    if (!sc_cli_apply_styles(ctx, &styles, slots, SC_CLI_TABLE_SIZE(slots))) {
        return SC_CLI_EXIT_ERROR;
    }

    if (optind < argc) {
        return run_spin_command(ctx, &args, argv + optind);
    }
    return run_spin_stdin(&args);
}

/* Forks COMMAND and animates the spinner on /dev/tty until it exits.
   Falls back to plain execution when there is no terminal to draw on. */
static int run_spin_command(ScCliCtx *ctx, const SpinArgs *args,
                            char **command) {
    pid_t pid = fork();
    if (pid < 0) {
        return sc_cli_error(ctx, "cannot fork: %s", strerror(errno));
    }
    if (pid == 0) {
        _exit(run_child(command));
    }

    /* The spinner is a stream widget; route it to the terminal (opened
       only in the parent, after the fork) so the child's stdout/stderr
       stay clean for pipes and redirection. */
    FILE *tty = fopen("/dev/tty", "w");
    if (tty == NULL) {
        /* No terminal: just wait for the child and propagate its status. */
        int status = 0;
        waitpid(pid, &status, 0);
        return exit_code_from_status(status);
    }

    sc_output_set_stream(tty);
    ScSpinner *spinner = sc_spinner_new(args->title, args->opts);
    int exit_code = wait_with_spinner(pid, spinner);

    sc_spinner_finish(spinner, exit_code == 0, args->title);
    sc_spinner_free(spinner);
    sc_output_set_stream(NULL);
    fclose(tty);
    return exit_code;
}

/* Child process body: exec the command; report exec failure as exit 127. */
static int run_child(char **command) {
    enum { EXEC_FAILURE_EXIT = 127 };
    execvp(command[0], command);
    fprintf(stderr, "sparcli spin: cannot run '%s': %s\n", command[0],
            strerror(errno));
    return EXEC_FAILURE_EXIT;
}

/* Ticks the spinner until the child exits; returns its exit code. */
static int wait_with_spinner(pid_t pid, ScSpinner *spinner) {
    while (true) {
        int   status = 0;
        pid_t done   = waitpid(pid, &status, WNOHANG);
        if (done == pid) {
            return exit_code_from_status(status);
        }
        if (done < 0 && errno != EINTR) {
            return SC_CLI_EXIT_ERROR;
        }
        sc_spinner_tick(spinner);
        sleep_ms(SC_CLI_SPIN_TICK_MS);
    }
}

/* Maps a waitpid status to a shell exit code (128+N for signals). */
static int exit_code_from_status(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return SC_CLI_SIGNAL_EXIT_BASE + WTERMSIG(status);
    }
    return SC_CLI_EXIT_ERROR;
}

/* Spins while consuming stdin to EOF (for use at the end of a pipe). */
static int run_spin_stdin(const SpinArgs *args) {
    FILE *tty = fopen("/dev/tty", "w");
    if (tty != NULL) {
        sc_output_set_stream(tty);
    }

    ScSpinner *spinner = (tty != NULL)
                             ? sc_spinner_new(args->title, args->opts)
                             : NULL;

    char buffer[SC_CLI_LINE_BUFFER];
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        if (spinner != NULL) {
            sc_spinner_tick(spinner);
        }
    }

    if (spinner != NULL) {
        sc_spinner_finish(spinner, true, args->title);
        sc_spinner_free(spinner);
    }
    if (tty != NULL) {
        sc_output_set_stream(NULL);
        fclose(tty);
    }
    return SC_CLI_EXIT_OK;
}


/* ── progress ───────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli progress`. */
typedef struct ProgressArgs {
    ScProgressBarOpts opts;
    const char       *label;
    double            total;
} ProgressArgs;

static int run_progress(ScCliCtx *ctx, const ProgressArgs *args);

static const char PROGRESS_USAGE[] =
    "Usage: ... | sparcli progress [options]\n"
    "\n"
    "Draw a progress bar driven by numeric values read from stdin (one\n"
    "per line). With --total N each value is taken as 'N done'; without\n"
    "it values are ratios between 0 and 1. Non-numeric lines are\n"
    "ignored. The bar finishes when stdin closes.\n"
    "\n"
    "Example:\n"
    "  for i in $(seq 100); do do_work; echo $i; done | \\\n"
    "      sparcli progress --total 100 --label Working\n"
    "\n"
    "Options:\n"
    "  --total N                  Value that counts as 100%\n"
    "  --type STYLE               block|ascii|line|shaded\n"
    "  --label TEXT               Label left of the bar\n"
    "  --color COLOR              Fill color\n"
    "  --empty-color COLOR        Color of the unfilled part\n"
    "  --left-cap STR             Left bracket (e.g. '[')\n"
    "  --right-cap STR            Right bracket (e.g. ']')\n"
    "  --show-value               Append (value/max) after the percent\n"
    "  --no-percent               Hide the percentage\n"
    "  --bar-width N              Inner bar width (0 = auto)\n"
    "  --label-width N            Fixed label column width\n"
    "  --width N                  Total line width (0 = terminal width)\n"
    "  --thresholds               Color the fill by ratio\n"
    "  --threshold-mid RATIO      Low/mid boundary (default 0.5)\n"
    "  --threshold-high RATIO     Mid/high boundary (default 0.75)\n"
    "  --threshold-low-color COLOR   Fill color below mid\n"
    "  --threshold-mid-color COLOR   Fill color between mid and high\n"
    "  --threshold-high-color COLOR  Fill color at/above high\n"
    SC_CLI_COMMON_USAGE
    "\n"
    "--style elements: label\n";

int sc_cli_cmd_progress(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_TOTAL = SC_CLI_OPT_CMD_BASE,
        OPT_TYPE,
        OPT_LABEL,
        OPT_COLOR,
        OPT_EMPTY_COLOR,
        OPT_LEFT_CAP,
        OPT_RIGHT_CAP,
        OPT_SHOW_VALUE,
        OPT_WIDTH,
        OPT_BAR_WIDTH,
        OPT_LABEL_WIDTH,
        OPT_NO_PERCENT,
        OPT_THRESHOLDS,
        OPT_THRESHOLD_MID,
        OPT_THRESHOLD_HIGH,
        OPT_THRESHOLD_LOW_COLOR,
        OPT_THRESHOLD_MID_COLOR,
        OPT_THRESHOLD_HIGH_COLOR,
    };
    static const struct option longopts[] = {
        { "total",                required_argument, NULL, OPT_TOTAL },
        { "type",                 required_argument, NULL, OPT_TYPE },
        { "label",                required_argument, NULL, OPT_LABEL },
        { "color",                required_argument, NULL, OPT_COLOR },
        { "empty-color",          required_argument, NULL, OPT_EMPTY_COLOR },
        { "left-cap",             required_argument, NULL, OPT_LEFT_CAP },
        { "right-cap",            required_argument, NULL, OPT_RIGHT_CAP },
        { "show-value",           no_argument,       NULL, OPT_SHOW_VALUE },
        { "width",                required_argument, NULL, OPT_WIDTH },
        { "bar-width",            required_argument, NULL, OPT_BAR_WIDTH },
        { "label-width",          required_argument, NULL, OPT_LABEL_WIDTH },
        { "no-percent",           no_argument,       NULL, OPT_NO_PERCENT },
        { "thresholds",           no_argument,       NULL, OPT_THRESHOLDS },
        { "threshold-mid",        required_argument, NULL, OPT_THRESHOLD_MID },
        { "threshold-high",       required_argument, NULL, OPT_THRESHOLD_HIGH },
        { "threshold-low-color",  required_argument, NULL,
          OPT_THRESHOLD_LOW_COLOR },
        { "threshold-mid-color",  required_argument, NULL,
          OPT_THRESHOLD_MID_COLOR },
        { "threshold-high-color", required_argument, NULL,
          OPT_THRESHOLD_HIGH_COLOR },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    ProgressArgs   args   = { .opts = { .show_percent = true } };
    ScCliStyleArgs styles = { 0 };
    int            opt    = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_TOTAL:
            if (!sc_cli_parse_double(optarg, &args.total)) {
                return sc_cli_error(ctx, "invalid total '%s'", optarg);
            }
            break;
        case OPT_TYPE:
            if (!sc_cli_parse_progress(optarg, &args.opts.type)) {
                return sc_cli_error(ctx, "invalid type '%s'", optarg);
            }
            break;
        case OPT_LABEL:
            args.label = optarg;
            break;
        case OPT_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.fill_color)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_EMPTY_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.empty_color)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_LEFT_CAP:
            args.opts.left_cap = optarg;
            break;
        case OPT_RIGHT_CAP:
            args.opts.right_cap = optarg;
            break;
        case OPT_SHOW_VALUE:
            args.opts.show_value = true;
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case OPT_BAR_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.bar_width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case OPT_LABEL_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.label_width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case OPT_NO_PERCENT:
            args.opts.show_percent = false;
            break;
        case OPT_THRESHOLDS:
            args.opts.thresholds.enabled = true;
            break;
        case OPT_THRESHOLD_MID:
            if (!sc_cli_parse_double(optarg, &args.opts.thresholds.mid)) {
                return sc_cli_error(ctx, "invalid ratio '%s'", optarg);
            }
            args.opts.thresholds.enabled = true;
            break;
        case OPT_THRESHOLD_HIGH:
            if (!sc_cli_parse_double(optarg, &args.opts.thresholds.high)) {
                return sc_cli_error(ctx, "invalid ratio '%s'", optarg);
            }
            args.opts.thresholds.enabled = true;
            break;
        case OPT_THRESHOLD_LOW_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.thresholds.color_low)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            args.opts.thresholds.enabled = true;
            break;
        case OPT_THRESHOLD_MID_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.thresholds.color_mid)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            args.opts.thresholds.enabled = true;
            break;
        case OPT_THRESHOLD_HIGH_COLOR:
            if (!sc_cli_parse_color(optarg,
                                    &args.opts.thresholds.color_high)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            args.opts.thresholds.enabled = true;
            break;
        case SC_CLI_OPT_STYLE:
            sc_cli_style_collect(&styles, optarg);
            break;
        case SC_CLI_OPT_HELP:
            fputs(PROGRESS_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    const ScCliStyleSlot slots[] = {
        { "label", &args.opts.label_style },
    };
    if (!sc_cli_apply_styles(ctx, &styles, slots, SC_CLI_TABLE_SIZE(slots))) {
        return SC_CLI_EXIT_ERROR;
    }

    return run_progress(ctx, &args);
}

static int run_progress(ScCliCtx *ctx, const ProgressArgs *args) {
    ScProgressBar *bar = sc_progressbar_new(args->opts);
    if (bar == NULL) {
        return sc_cli_error(ctx, "out of memory");
    }
    if (args->label != NULL) {
        sc_progressbar_set_label(bar, args->label);
    }

    /* Each numeric stdin line advances the bar; everything else is
       ignored so interleaved log output does not break the animation. */
    double last_value = 0.0;
    char   buffer[SC_CLI_LINE_BUFFER];
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        char  *end   = NULL;
        double value = strtod(buffer, &end);
        if (end == buffer) {
            continue;
        }
        last_value = value;
        sc_progressbar_draw(bar, value, args->total);
    }

    sc_progressbar_finish(bar, last_value, args->total);
    sc_progressbar_free(bar);
    return SC_CLI_EXIT_OK;
}


/* ── helpers ────────────────────────────────────────────────────────────── */

static void sleep_ms(int milliseconds) {
    struct timespec duration = {
        .tv_sec  = milliseconds / SC_CLI_MS_PER_SECOND,
        .tv_nsec = (long)(milliseconds % SC_CLI_MS_PER_SECOND)
                   * SC_CLI_NS_PER_MS,
    };
    nanosleep(&duration, NULL);
}
