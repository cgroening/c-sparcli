#include "test_input.h"
#include "sparcli.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>


#define N_THREADS 8
#define ITERS     400

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

void test_threads(void) {
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
