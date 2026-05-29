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
        const char *colored =
            "\033[1;32mHello\033[0m, \033[31mworld\033[0m!";
        char *plain = sc_strip_ansi(colored);
        printf("Before: %s\n", colored);
        printf("After:  %s\n", plain);
        free(plain);
    }

    printf("\n");

    /* ── 2. sc_truncate ── */
    printf("--- Util 2. sc_truncate ---\n");
    {
        const char *long_string =
            "This is a very long string that needs truncation";
        char *with_dots = sc_truncate(long_string, 20, "...");
        char *with_ellipsis = sc_truncate(
            long_string, 20, "\xe2\x80\xa6"   /* … U+2026 */
        );
        char *short_unchanged = sc_truncate("Short", 20, "...");

        printf("Original (len=%d): %s\n",
            (int)strlen(long_string), long_string);
        printf("20 cols + '...':   %s\n", with_dots);
        printf("20 cols + '\xe2\x80\xa6':    %s\n", with_ellipsis);
        printf("Short unchanged:   %s\n", short_unchanged);

        free(with_dots);
        free(with_ellipsis);
        free(short_unchanged);
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
