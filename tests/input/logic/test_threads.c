#include "test_input.h"
#include "sparcli.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define N_THREADS 8
#define ITERS     400

/** Log records per thread in the concurrent-logging test. */
#define LOG_ITERS 200

/*
 * Each worker captures its own distinct string many times. The capture API
 * swaps the (thread-local) output stream internally; if that state were shared
 * across threads, concurrent captures would clobber each other and produce
 * wrong/garbled results. A non-NULL return signals a mismatch.
 */
static void *worker(void *arg) {
    long id = (long)arg;
    char want[32];
    snprintf(want, sizeof want, "thread-%ld-payload", id);

    for (int i = 0; i < ITERS; i++) {
        ScRendered *r = sc_capture_str(want);
        int ok = r && r->line_count == 1 && r->lines[0]
              && strcmp(r->lines[0], want) == 0;
        sc_rendered_free(r);
        if (!ok) { return (void *)1; }
    }
    return (void *)0;
}

/*
 * Each worker emits records through the global logger into a shared file
 * sink. The global config is set up before the threads start (set-once
 * contract); the emits themselves must be race-free and line-atomic.
 */
static void *log_worker(void *arg) {
    long id = (long)arg;
    for (int i = 0; i < LOG_ITERS; i++) {
        sc_log_info("thread %ld record %d", id, i);
    }
    return (void *)0;
}

/** Counts complete, well-formed record lines in the shared log file. */
static long count_complete_lines(const char *path) {
    FILE *stream = fopen(path, "r");
    if (!stream) { return -1; }

    long complete = 0;
    char line[512];
    while (fgets(line, sizeof line, stream)) {
        // Every record line starts with the level tag and ends with \n
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n'
            && strstr(line, "INFO") != NULL
            && strstr(line, "record") != NULL) {
            complete++;
        }
    }
    fclose(stream);
    return complete;
}

static void check_capture_threads(void) {
    pthread_t th[N_THREADS];
    int started = 0;
    for (long i = 0; i < N_THREADS; i++) {
        if (pthread_create(&th[i], NULL, worker, (void *)i) == 0) { started++; }
    }
    long fails = 0;
    for (int i = 0; i < started; i++) {
        void *rv = NULL;
        pthread_join(th[i], &rv);
        fails += (rv != NULL);
    }
    CHECK(started == N_THREADS, "all worker threads started");
    CHECK(fails == 0,
          "concurrent sc_capture across threads is race-free "
          "(thread-local output)");
}

static void check_logging_threads(void) {
    // Configure the global logger BEFORE spawning threads (set-once config)
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) { tmp = "/tmp"; }
    char path[512];
    snprintf(path, sizeof path, "%s/sparcli-threads-%d.log", tmp, getpid());
    unlink(path);

    sc_log_reset();
    sc_log_set_level(SC_LOG_OFF);   // keep the test terminal quiet
    sc_log_set_opts((ScLoggerOpts){ .hide_timestamps = true });
    if (!sc_log_add_file(path, SC_LOG_DEBUG)) {
        CHECK(false, "concurrent logging: shared file sink opens");
        sc_log_reset();
        return;
    }

    pthread_t th[N_THREADS];
    int started = 0;
    for (long i = 0; i < N_THREADS; i++) {
        if (pthread_create(&th[i], NULL, log_worker, (void *)i) == 0) {
            started++;
        }
    }
    for (int i = 0; i < started; i++) {
        pthread_join(th[i], NULL);
    }
    sc_log_reset();   // closes + flushes the file sink

    CHECK(started == N_THREADS, "all logging threads started");
    long complete = count_complete_lines(path);
    CHECK(complete == (long)started * LOG_ITERS,
          "concurrent sc_log to a shared file sink is race-free "
          "(every record is one complete line)");

    unlink(path);
}

void test_threads(void) {
    check_capture_threads();
    check_logging_threads();
}
