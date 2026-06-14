#include "input/sparcli_editor.h"

#ifdef _WIN32

#include "input/sparcli_term.h" /* sc_input_available (honors SPARCLI_NO_TTY) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* Editor command: explicit cmd wins, then $VISUAL/$EDITOR, then notepad. */
static const char *win_resolve_editor(const char *cmd) {
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
    return "notepad";
}

/* UTF-8 -> freshly allocated UTF-16 (caller frees), or NULL on failure. */
static wchar_t *utf8_to_wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) {
        return NULL;
    }
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof *w);
    if (!w) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0) {
        free(w);
        return NULL;
    }
    return w;
}

int sc_edit_file(const char *cmd, const char *path) {
    if (!path || !path[0]) {
        return -1;
    }
    if (!sc_input_available()) {
        return -1; /* no console / SPARCLI_NO_TTY */
    }

    /* Build the command line `<editor> "<path>"` as one UTF-8 string, then
       convert to a writable UTF-16 buffer (CreateProcessW may modify it). */
    const char *editor = win_resolve_editor(cmd);
    size_t need = strlen(editor) + strlen(path) + 4; /* ' ' + 2 quotes + NUL */
    char *line = (char *)malloc(need);
    if (!line) {
        return -1;
    }
    snprintf(line, need, "%s \"%s\"", editor, path);
    wchar_t *wline = utf8_to_wide(line);
    free(line);
    if (!wline) {
        return -1;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);

    /* No redirection and no CREATE_NEW_CONSOLE: the child inherits this
       console, so an interactive editor drives the terminal directly. */
    BOOL ok = CreateProcessW(NULL, wline, NULL, NULL, FALSE, 0, NULL, NULL,
                             &si, &pi);
    if (!ok) {
        DWORD err = GetLastError();
        free(wline);
        return (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
                   ? 127
                   : -1;
    }
    free(wline);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &code)) {
        code = (DWORD)-1;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

#else /* POSIX */

#include "input/sparcli_term.h" /* sc_input_available */
#include "input_internal.h"     /* sc_editor_run_child, sc_editor_resolve */

int sc_edit_file(const char *cmd, const char *path) {
    if (!path || !path[0]) {
        return -1;
    }
    if (!sc_input_available()) {
        return -1; /* no controlling terminal / SPARCLI_NO_TTY */
    }
    return sc_editor_run_child(sc_editor_resolve(cmd), path);
}

#endif
