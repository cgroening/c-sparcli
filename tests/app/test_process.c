#include "test_app.h"
#include "sparcli.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


static bool contains(const char *haystack, const char *needle) {
    return haystack != NULL && strstr(haystack, needle) != NULL;
}


/**
 * Subprocess helper: stdout/stderr capture, exit codes, stdin feeding,
 * stderr merging, the no-such-command path, sanitization of captured
 * output and large-output deadlock-freedom. Uses portable tools
 * (`echo`, `cat`, `printf`, `false`, `sh`) as deterministic children.
 */
void test_process(void) {
    /* ── stdout capture + exit code ── */
    {
        const char *argv[] = { "echo", "hello world", NULL };
        ScProcResult r = sc_run(argv, (ScProcOpts){ 0 });
        CHECK(r.ok, "process: echo spawns and is reaped");
        CHECK(r.exit_code == 0, "process: echo exits 0");
        CHECK(contains(r.out, "hello world"), "process: stdout captured");
        sc_proc_result_free(&r);
    }

    /* ── stdin → stdout via cat ── */
    {
        const char *argv[] = { "cat", NULL };
        ScProcResult r = sc_run(argv, (ScProcOpts){ .input = "piped in\n" });
        CHECK(contains(r.out, "piped in"), "process: stdin fed to child");
        sc_proc_result_free(&r);
    }

    /* ── separate stderr ── */
    {
        const char *argv[] = { "sh", "-c", "echo OUT; echo ERR 1>&2", NULL };
        ScProcResult r = sc_run(argv, (ScProcOpts){ 0 });
        CHECK(contains(r.out, "OUT") && !contains(r.out, "ERR"),
              "process: stdout has only stdout");
        CHECK(contains(r.err, "ERR"), "process: stderr captured separately");
        sc_proc_result_free(&r);
    }

    /* ── merged stderr ── */
    {
        const char *argv[] = { "sh", "-c", "echo OUT; echo ERR 1>&2", NULL };
        ScProcResult r = sc_run(argv, (ScProcOpts){ .merge_stderr = true });
        CHECK(contains(r.out, "OUT") && contains(r.out, "ERR"),
              "process: merge_stderr folds stderr into stdout");
        CHECK(r.err == NULL, "process: merged stderr leaves err NULL");
        sc_proc_result_free(&r);
    }

    /* ── non-zero exit ── */
    {
        const char *argv[] = { "false", NULL };
        ScProcResult r = sc_run(argv, (ScProcOpts){ 0 });
        CHECK(r.ok && r.exit_code == 1, "process: false reports exit 1");
        sc_proc_result_free(&r);
    }

    /* ── command not found ── */
    {
        const char *argv[] = { "sparcli-no-such-binary-xyz", NULL };
        ScProcResult r = sc_run(argv, (ScProcOpts){ 0 });
        CHECK(r.ok && r.exit_code == 127,
              "process: missing command yields exit 127");
        sc_proc_result_free(&r);
    }

    /* ── sanitization strips ANSI by default, raw keeps it ── */
    {
        const char *argv[] = { "printf", "\\033[31mred\\033[0m", NULL };
        ScProcResult clean = sc_run(argv, (ScProcOpts){ 0 });
        CHECK(contains(clean.out, "red") && !contains(clean.out, "\033"),
              "process: captured output sanitized by default");
        sc_proc_result_free(&clean);

        ScProcResult raw = sc_run(argv, (ScProcOpts){ .raw = true });
        CHECK(contains(raw.out, "\033["), "process: raw keeps escape bytes");
        sc_proc_result_free(&raw);
    }

    /* ── large output must not deadlock ── */
    {
        const char *argv[] = { "sh", "-c", "yes ABCDEFGH | head -n 20000",
                               NULL };
        ScProcResult r = sc_run(argv, (ScProcOpts){ 0 });
        CHECK(r.ok && r.out_len > 100000,
              "process: large stdout drained without deadlock");
        sc_proc_result_free(&r);
    }

    /* ── working directory ── */
    {
        const char *argv[] = { "pwd", NULL };
        ScProcResult r = sc_run(argv, (ScProcOpts){ .cwd = "/" });
        CHECK(contains(r.out, "/"), "process: cwd applied (pwd in /)");
        sc_proc_result_free(&r);
    }

    /* ── invalid argv + NULL-safe free ── */
    {
        ScProcResult r = sc_run(NULL, (ScProcOpts){ 0 });
        CHECK(!r.ok, "process: NULL argv reports failure");
        sc_proc_result_free(&r);
        sc_proc_result_free(NULL);
        CHECK(true, "process: sc_proc_result_free(NULL) is safe");
    }
}
