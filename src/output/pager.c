#include "sparcli.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


/** Max whitespace-separated tokens in a pager command (+ NULL). */
#define PAGER_MAX_ARGS 16

/** Default pager when neither `opts.command` nor `$PAGER` is set. */
#define PAGER_DEFAULT_COMMAND "less -R"


/** Pager session state (opaque to callers). */
struct ScPager {
    FILE *pipe_stream;      /**< Write end of the pipe to the pager. */
    FILE *previous_stream;  /**< Output stream to restore on end. */
    pid_t child_pid;        /**< Pager process id. */

    /** `SIGPIPE` disposition to restore on end. */
    struct sigaction previous_sigpipe;

    /** `false` = no-op session (non-TTY or startup failure). */
    bool active;
};


// Forward declarations indented to reflect call hierarchy
static bool start_pager_process(ScPager *pager, ScPagerOpts opts);
    static const char *resolve_pager_command(const char *command);
    static int build_pager_argv(char *command, char **argv);
    static void exec_pager_child(int read_fd, char *const argv[]);


ScPager *sc_pager_begin(ScPagerOpts opts) {
    ScPager *pager = calloc(1, sizeof(ScPager));
    if (!pager) { return NULL; }

    // Off-terminal output (pipe, file, capture) is not paged by default
    FILE *current = sc_output_stream();
    int current_fd = fileno(current);
    if (!opts.always && (current_fd < 0 || !isatty(current_fd))) {
        return pager;
    }

    if (!start_pager_process(pager, opts)) {
        return pager;   // degrade to a no-op session
    }

    // Quitting the pager early must not kill the program via SIGPIPE
    struct sigaction ignore_action = { .sa_handler = SIG_IGN };
    sigemptyset(&ignore_action.sa_mask);
    ignore_action.sa_flags = 0;
    sigaction(SIGPIPE, &ignore_action, &pager->previous_sigpipe);

    pager->previous_stream = current;
    pager->active = true;
    sc_output_set_stream(pager->pipe_stream);
    return pager;
}

int sc_pager_end(ScPager *pager) {
    if (!pager) { return 0; }
    if (!pager->active) {
        free(pager);
        return 0;
    }

    sc_output_set_stream(pager->previous_stream);

    // Closing the write end sends EOF, letting the pager finish
    fflush(pager->pipe_stream);
    fclose(pager->pipe_stream);

    int status = 0;
    int exit_code = -1;
    while (waitpid(pager->child_pid, &status, 0) < 0) {
        if (errno != EINTR) {
            status = -1;
            break;
        }
    }
    if (status >= 0 && WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    }

    sigaction(SIGPIPE, &pager->previous_sigpipe, NULL);
    free(pager);
    return exit_code;
}

/**
 * Forks the pager process and connects a pipe to its stdin. On success
 * fills `pager->pipe_stream` / `pager->child_pid` and returns `true`; on
 * any failure all resources are released and `false` is returned.
 */
static bool start_pager_process(ScPager *pager, ScPagerOpts opts) {
    char *command_copy = strdup(resolve_pager_command(opts.command));
    if (!command_copy) { return false; }

    char *argv[PAGER_MAX_ARGS + 1];
    if (build_pager_argv(command_copy, argv) == 0) {
        free(command_copy);
        return false;
    }

    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        free(command_copy);
        return false;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipe_fds[1]);
        exec_pager_child(pipe_fds[0], argv);
    }
    free(command_copy);
    close(pipe_fds[0]);
    if (pid < 0) {
        close(pipe_fds[1]);
        return false;
    }

    FILE *pipe_stream = fdopen(pipe_fds[1], "w");
    if (!pipe_stream) {
        close(pipe_fds[1]);
        while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) { /* retry */ }
        return false;
    }

    pager->pipe_stream = pipe_stream;
    pager->child_pid = pid;
    return true;
}

/**
 * Resolves the pager command string: explicit `command` (non-empty) wins,
 * then `$PAGER`, then the built-in default.
 */
static const char *resolve_pager_command(const char *command) {
    if (command && command[0]) {
        return command;
    }
    const char *env_pager = getenv("PAGER");
    if (env_pager && env_pager[0]) {
        return env_pager;
    }
    return PAGER_DEFAULT_COMMAND;
}

/**
 * Tokenizes `command` (a mutable copy) on whitespace into `argv` and
 * appends a NULL terminator. Returns argc, or 0 when there are no tokens.
 * Shell-free: no quoting or expansion.
 */
static int build_pager_argv(char *command, char **argv) {
    int argc = 0;
    char *save = NULL;
    for (char *token = strtok_r(command, " \t", &save);
         token && argc < PAGER_MAX_ARGS;
         token = strtok_r(NULL, " \t", &save)) {
        argv[argc++] = token;
    }
    argv[argc] = NULL;
    return argc;
}

/**
 * Child half of `start_pager_process`: stdin = pipe read end, then
 * `execvp` the pager (`cat` as a last resort so output is never lost).
 * Only async-signal-safe calls; never returns.
 */
static void exec_pager_child(int read_fd, char *const argv[]) {
    dup2(read_fd, STDIN_FILENO);
    if (read_fd != STDIN_FILENO) { close(read_fd); }

    execvp(argv[0], argv);
    /* First choice failed → pass everything through unpaged. */
    char *cat_argv[] = { "cat", NULL };
    execvp("cat", cat_argv);
    _exit(127);
}
