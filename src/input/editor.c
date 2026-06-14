#include "input_internal.h"
#include "core/sanitize_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <io.h>
#else
#  include <errno.h>
#  include <fcntl.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif


/* Max whitespace-separated tokens in an editor command (+ file + NULL). */
#define EDITOR_MAX_ARGS 16


/**
 * Resolves the editor command string: explicit `cmd` (non-empty) wins, then
 * `$VISUAL`, `$EDITOR`, then the platform default (nvim/vi on POSIX, notepad
 * on Windows).
 */
const char *sc_editor_resolve(const char *cmd) {
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
#ifdef _WIN32
    return "notepad";
#else
    return "nvim";   // execvp falls through to "vi" below if this is missing
#endif
}


#ifdef _WIN32
/* ── Windows: temp file via GetTempFileNameW, editor via CreateProcessW ───── */

static wchar_t *utf8_to_wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) { return NULL; }
    wchar_t *w = malloc((size_t)n * sizeof *w);
    if (!w) { return NULL; }
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0) {
        free(w);
        return NULL;
    }
    return w;
}

static char *make_temp(const char *initial, const char *suffix,
                       size_t *out_len) {
    if (!suffix) { suffix = ""; }
    wchar_t dir[MAX_PATH];
    wchar_t wbase[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, dir);
    if (n == 0 || n > MAX_PATH) { return NULL; }
    if (GetTempFileNameW(dir, L"scl", 0, wbase) == 0) { return NULL; }

    int u = WideCharToMultiByte(CP_UTF8, 0, wbase, -1, NULL, 0, NULL, NULL);
    if (u <= 0) { DeleteFileW(wbase); return NULL; }
    char *path = malloc((size_t)u + strlen(suffix));
    if (!path) { DeleteFileW(wbase); return NULL; }
    WideCharToMultiByte(CP_UTF8, 0, wbase, -1, path, u, NULL, NULL);
    if (suffix[0]) {
        /* Want the suffix (e.g. ".md") for editor filetype detection: drop the
         * generated .tmp file and use the suffixed name instead. */
        DeleteFileW(wbase);
        strcat(path, suffix);
    }

    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    size_t len = initial ? strlen(initial) : 0;
    if (len && fwrite(initial, 1, len, f) != len) {
        fclose(f);
        remove(path);
        free(path);
        return NULL;
    }
    fclose(f);
    *out_len = len;
    return path;
}

/**
 * Runs the editor on `file` with the current console inherited (no
 * redirection), so an interactive editor drives the terminal directly.
 * Returns the child's exit code, 127 when the editor is not found, or -1.
 */
int sc_editor_run_child(const char *cmd, const char *file) {
    size_t need = strlen(cmd) + strlen(file) + 4;   /* ' ' + 2 quotes + NUL */
    char *line = malloc(need);
    if (!line) { return -1; }
    snprintf(line, need, "%s \"%s\"", cmd, file);
    wchar_t *wline = utf8_to_wide(line);
    free(line);
    if (!wline) { return -1; }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);

    BOOL ok = CreateProcessW(NULL, wline, NULL, NULL, FALSE, 0, NULL, NULL,
                             &si, &pi);
    free(wline);
    if (!ok) {
        DWORD err = GetLastError();
        return (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
                   ? 127 : -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &code)) {
        code = (DWORD)-1;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

#else
/* ── POSIX: temp file via mkstemp(s), editor via fork + execvp ────────────── */

/**
 * Creates a 0600 temp file seeded with `initial`. Returns a heap path (caller
 * frees) and the written length, or NULL on failure.
 */
static char *make_temp(const char *initial, const char *suffix,
                       size_t *out_len) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) {
        dir = "/tmp";
    }
    if (!suffix) { suffix = ""; }
    size_t suffix_len = strlen(suffix);
    size_t need = strlen(dir) + sizeof("/sparcli-edit-XXXXXX") + suffix_len;
    char *path = malloc(need);
    if (!path) {
        return NULL;
    }
    snprintf(path, need, "%s/sparcli-edit-XXXXXX%s", dir, suffix);

    // mkstemps keeps the trailing `suffix` (e.g. ".md") so editors can detect
    // the filetype; plain mkstemp when there is none.
    int fd = suffix_len ? mkstemps(path, (int)suffix_len)
                        : mkstemp(path);   // both create with mode 0600
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
 * Tokenizes `cmd` (a mutable copy) on whitespace into `argv`, then appends
 * `file` and a NULL terminator. Returns argc, or 0 when `cmd` has no tokens.
 * Shell-free: no quoting or expansion.
 */
static int build_editor_argv(char *cmd, char **argv, const char *file) {
    int argc = 0;
    char *save = NULL;
    for (char *tok = strtok_r(cmd, " \t", &save);
         tok && argc < EDITOR_MAX_ARGS;
         tok = strtok_r(NULL, " \t", &save)) {
        argv[argc++] = tok;
    }
    if (argc == 0) {
        return 0;
    }
    argv[argc++] = (char *)file;
    argv[argc] = NULL;
    return argc;
}

/**
 * Child half of `sc_editor_run_child`: attach stdio to the controlling
 * terminal, restore
 * the inherited signal mask, then `execvp` the editor (`vi` as a last resort).
 * Only async-signal-safe calls; never returns.
 */
static void exec_editor_child(
    char *const argv[], const sigset_t *orig_mask, const char *file
) {
    int tty = open("/dev/tty", O_RDWR);
    if (tty >= 0) {
        dup2(tty, STDIN_FILENO);
        dup2(tty, STDOUT_FILENO);
        dup2(tty, STDERR_FILENO);
        if (tty > STDERR_FILENO) { close(tty); }
    }
    sigprocmask(SIG_SETMASK, orig_mask, NULL);
    execvp(argv[0], argv);
    /* First choice failed → last-resort vi with just the file. */
    char *vi_argv[] = { "vi", (char *)file, NULL };
    execvp("vi", vi_argv);
    _exit(127);
}

/**
 * Forks a child that runs the editor on `file` with its stdio on the
 * controlling terminal. Returns the child's exit status (0 = clean), or -1 on
 * fork/setup failure.
 */
int sc_editor_run_child(const char *cmd, const char *file) {
    char *cmd_copy = strdup(cmd);
    if (!cmd_copy) {
        return -1;
    }
    char *argv[EDITOR_MAX_ARGS + 2];
    if (build_editor_argv(cmd_copy, argv, file) == 0) {
        free(cmd_copy);
        return -1;
    }

    /* Reset the signals an editor manages to default *in the parent* (sigaction
     * is not async-signal-safe after fork), with them blocked across the fork
     * so the parent is not disturbed in that window. The child only unblocks
     * (async-signal-safe) and inherits the default dispositions. */
    static const int sigs[] = {
        SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGTSTP, SIGCONT, SIGWINCH
    };
    const size_t n_sigs = sizeof sigs / sizeof sigs[0];
    struct sigaction saved[sizeof sigs / sizeof sigs[0]];
    struct sigaction dfl;
    sigemptyset(&dfl.sa_mask);
    dfl.sa_flags = 0;
    dfl.sa_handler = SIG_DFL;
    sigset_t block, orig_mask;
    sigemptyset(&block);
    for (size_t i = 0; i < n_sigs; i++) { sigaddset(&block, sigs[i]); }
    sigprocmask(SIG_BLOCK, &block, &orig_mask);
    for (size_t i = 0; i < n_sigs; i++) { sigaction(sigs[i], &dfl, &saved[i]); }

    pid_t pid = fork();
    if (pid == 0) {
        exec_editor_child(argv, &orig_mask, file);
    }

    /* Parent (fork succeeded or failed): restore dispositions + mask at
       once. */
    for (size_t i = 0; i < n_sigs; i++) { sigaction(sigs[i], &saved[i], NULL); }
    sigprocmask(SIG_SETMASK, &orig_mask, NULL);
    if (pid < 0) {
        free(cmd_copy);
        return -1;
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

#endif  /* _WIN32 */


/** Reads the file at `path` into a heap buffer (caller frees), or NULL. */
static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    for (;;) {
        if (len + 4096 + 1 > cap) {
            size_t ncap = cap * 2;
            char *grown = realloc(buf, ncap);
            if (!grown) {
                free(buf);
                fclose(f);
                return NULL;
            }
            buf = grown;
            cap = ncap;
        }
        size_t n = fread(buf + len, 1, 4096, f);
        len += n;
        if (n < 4096) {
            if (ferror(f)) {
                free(buf);
                fclose(f);
                return NULL;
            }
            break;   // EOF
        }
    }
    fclose(f);
    buf[len] = '\0';
    return buf;
}

bool sc_run_editor(const char *cmd, const char *initial, const char *suffix,
                   char **out) {
    if (!out) {
        return false;
    }
    size_t seed_len = 0;
    char *path = make_temp(initial, suffix, &seed_len);
    if (!path) {
        return false;
    }

    int status = sc_editor_run_child(sc_editor_resolve(cmd), path);
    bool ok = false;
    if (status == 0) {
        char *content = read_all(path);
        if (content) {
            // Editor output is keyboard-equivalent input: always remove
            // control bytes and escape sequences (never allow ANSI), so
            // a malicious file cannot inject terminal escape codes.
            *out = sc_sanitize_copy(content, false);
            free(content);
            ok = *out != NULL;
        }
    }

    remove(path);
    free(path);
    return ok;
}
