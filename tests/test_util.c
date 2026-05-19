#include "sparcli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void test_util(void) {
    printf("\n");

    /* ── 1. sc_strip_ansi ── */
    printf("--- Util 1. sc_strip_ansi ---\n");
    {
        const char *colored = "\033[1;32mHello\033[0m, \033[31mworld\033[0m!";
        char       *plain   = sc_strip_ansi(colored);
        printf("Before: %s\n", colored);
        printf("After:  %s\n", plain);
        free(plain);
    }

    printf("\n");

    /* ── 2. sc_truncate ── */
    printf("--- Util 2. sc_truncate ---\n");
    {
        const char *long_str = "This is a very long string that needs truncation";
        char *t1 = sc_truncate(long_str, 20, "...");
        char *t2 = sc_truncate(long_str, 20, "\xe2\x80\xa6"); /* … U+2026 */
        char *t3 = sc_truncate("Short", 20, "...");           /* fits: no change */
        printf("Original (len=%d): %s\n", (int)strlen(long_str), long_str);
        printf("20 cols + '...':   %s\n", t1);
        printf("20 cols + '\xe2\x80\xa6':    %s\n", t2);
        printf("Short unchanged:   %s\n", t3);
        free(t1); free(t2); free(t3);
    }

    printf("\n");

    /* ── 3. sc_clear_line ── */
    printf("--- Util 3. sc_clear_line ---\n");
    {
        printf("This line will be cleared...");
        fflush(stdout);
        usleep(400000);
        sc_clear_line();
        printf("Line cleared and replaced!\n");
    }
}
