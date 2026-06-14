#include "core/sc_memstream.h"

#include <stdlib.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <wchar.h>
#endif


FILE *sc_memstream_open(ScMemStream *ms, char **buf, size_t *size) {
    ms->fp = NULL;
    ms->buf_out = buf;
    ms->size_out = size;
    *buf = NULL;
    *size = 0;

#ifdef _WIN32
    /* tmpfile() is unusable here: the UCRT/msvcrt implementation creates its
     * file in the drive root, which fails for a non-elevated process. Place
     * the file under %TEMP% instead. Mode "w+bTD": read/write, binary (exact
     * bytes - no newline translation, matching open_memstream), T = keep
     * cached in memory when possible, D = delete on close. */
    wchar_t dir[MAX_PATH];
    wchar_t path[MAX_PATH];
    DWORD len = GetTempPathW(MAX_PATH, dir);
    if (len == 0 || len > MAX_PATH) { return NULL; }
    if (GetTempFileNameW(dir, L"scm", 0, path) == 0) { return NULL; }
    ms->fp = _wfopen(path, L"w+bTD");
    if (!ms->fp) {
        DeleteFileW(path);
        return NULL;
    }
    return ms->fp;
#else
    ms->fp = open_memstream(buf, size);
    return ms->fp;
#endif
}


int sc_memstream_close(ScMemStream *ms) {
    if (!ms->fp) { return -1; }

#ifdef _WIN32
    if (fflush(ms->fp) != 0) { goto fail; }
    long end = ftell(ms->fp);
    if (end < 0) { goto fail; }

    size_t length = (size_t)end;
    char *out = malloc(length + 1);
    if (!out) { goto fail; }

    rewind(ms->fp);
    size_t got = fread(out, 1, length, ms->fp);
    out[got] = '\0';

    fclose(ms->fp);
    ms->fp = NULL;
    *ms->buf_out = out;
    *ms->size_out = got;
    return 0;

fail:
    fclose(ms->fp);
    ms->fp = NULL;
    return -1;
#else
    /* open_memstream populated the caller's buf/size on flush; fclose
     * finalizes them. */
    int rc = fclose(ms->fp);
    ms->fp = NULL;
    return rc == 0 ? 0 : -1;
#endif
}
