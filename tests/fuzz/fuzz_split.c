#include "sparcli.h"
#include "fuzz_common.h"

#include <stdio.h>
#include <string.h>

/**
 * Fuzz target for the line tokenizer: random byte sequences fed to
 * `sc_args_split` must never crash, leak, or trigger UB. REPL input lines
 * are the untrusted surface of every interactive shell built on the args
 * module.
 */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // The tokenizer expects a NUL-terminated line
    char *line = malloc(size + 1);
    if (!line) { return 0; }
    memcpy(line, data, size);
    line[size] = '\0';

    int argc = 0;
    char err[64];
    char **argv = sc_args_split("fuzz", line, &argc, err, sizeof err);

    if (argv) {
        // The contract: NULL-terminated, argc entries, argv[0] == "fuzz"
        if (argc < 1 || strcmp(argv[0], "fuzz") != 0 || argv[argc] != NULL) {
            abort();
        }
        // Every token must be a readable NUL-terminated string
        for (int i = 0; i < argc; i++) {
            (void)strlen(argv[i]);
        }
        sc_args_split_free(argv);
    } else {
        // Errors must report a reason and zero the count
        if (argc != 0 || err[0] == '\0') {
            abort();
        }
    }

    free(line);
    return 0;
}
