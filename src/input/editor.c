#include "input_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


/* Max whitespace-separated tokens in an editor command (+ file + NULL). */
#define EDITOR_MAX_ARGS 16


/**
 * Resolves the editor command string: explicit `cmd` (non-empty) wins, then
 * `$VISUAL`, `$EDITOR`, then "nvim", then "vi".
 */
static const char *resolve_editor(const char *cmd) {
    if (cmd && cmd[0]) {
        return cmd;
    }
    const char *v = getenv("VISUAL");
    if (v && v[0]) {
        return v;
    }
    v = getenv("EDITOR");
    if (v && v[0]) {
        return v;
    }
    return "nvim";   // execvp falls through to "vi" below if this is missing
}

/**
 * Creates a 0600 temp file seeded with `initial`. Returns a heap path (caller
 * frees) and the written length, or NULL on failure.
 */
static char *make_temp(const char *initial, size_t *out_len) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) {
        dir = "/tmp";
    }
    size_t need = strlen(dir) + sizeof("/sparcli-edit-XXXXXX");
    char *path = malloc(need);
    if (!path) {
        return NULL;
    }
    snprintf(path, need, "%s/sparcli-edit-XXXXXX", dir);

    int fd = mkstemp(path);   // creates with mode 0600
    if (fd < 0) {
        free(path);
        return NULL;
    }

    size_t len = initial ? strlen(initial) : 0;
    size_t off = 0;
    bool ok = true;
    while (off < len) {
        ssize_t w = write(fd, initial + off, len - off);
        if (w < 0) {
            if (errno == EINTR) { continue; }
            ok = false;
            break;
        }
        off += (size_t)w;
    }
    close(fd);
    if (!ok) {
        unlink(path);
        free(path);
        return NULL;
    }
    *out_len = len;
    return path;
}

/**
 * Splits `cmd` on whitespace into `argv` (NULL-terminated), appends `file`, and
 * runs `execvp` in a forked child whose stdio is the controlling terminal.
 * Returns the child's exit status (0 = clean), or -1 on fork/setup failure.
 */
static int run_child(const char *cmd, const char *file) {
    /* Tokenize a mutable copy of cmd (shell-free; no quoted args). */
    char *cmd_copy = strdup(cmd);
    if (!cmd_copy) {
        return -1;
    }
    char *argv[EDITOR_MAX_ARGS + 2];
    int argc = 0;
    for (char *tok = strtok(cmd_copy, " \t");
         tok && argc < EDITOR_MAX_ARGS;
         tok = strtok(NULL, " \t")) {
        argv[argc++] = tok;
    }
    if (argc == 0) {
        free(cmd_copy);
        return -1;
    }
    argv[argc++] = (char *)file;
    argv[argc] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        free(cmd_copy);
        return -1;
    }
    if (pid == 0) {
        /* Child: attach stdio to the controlling terminal, restore default
         * signal handling, then exec the editor. */
        int tty = open("/dev/tty", O_RDWR);
        if (tty >= 0) {
            dup2(tty, STDIN_FILENO);
            dup2(tty, STDOUT_FILENO);
            dup2(tty, STDERR_FILENO);
            if (tty > STDERR_FILENO) { close(tty); }
        }
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGWINCH, SIG_DFL);
        execvp(argv[0], argv);
        if (strcmp(argv[0], "nvim") == 0) { execvp("vi", argv); }
        _exit(127);   // exec failed
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            free(cmd_copy);
            return -1;
        }
    }
    free(cmd_copy);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;   // killed by a signal
}

/** Reads the whole file at `path` into a heap buffer (caller frees), or NULL. */
static char *read_all(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        close(fd);
        return NULL;
    }
    for (;;) {
        if (len + 4096 + 1 > cap) {
            size_t ncap = cap * 2;
            char *grown = realloc(buf, ncap);
            if (!grown) {
                free(buf);
                close(fd);
                return NULL;
            }
            buf = grown;
            cap = ncap;
        }
        ssize_t n = read(fd, buf + len, 4096);
        if (n < 0) {
            if (errno == EINTR) { continue; }
            free(buf);
            close(fd);
            return NULL;
        }
        if (n == 0) { break; }
        len += (size_t)n;
    }
    close(fd);
    buf[len] = '\0';
    return buf;
}

bool sc_run_editor(const char *cmd, const char *initial, char **out) {
    if (!out) {
        return false;
    }
    size_t seed_len = 0;
    char *path = make_temp(initial, &seed_len);
    if (!path) {
        return false;
    }

    int status = run_child(resolve_editor(cmd), path);
    bool ok = false;
    if (status == 0) {
        char *content = read_all(path);
        if (content) {
            *out = content;
            ok = true;
        }
    }

    unlink(path);
    free(path);
    return ok;
}
