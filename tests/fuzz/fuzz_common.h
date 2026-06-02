#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @file fuzz_common.h
 * @brief Shared driver for the portable random-input fuzz harnesses.
 *
 * Each harness defines `LLVMFuzzerTestOneInput(data, size)` (the libFuzzer
 * entry point) and includes this header, which provides a standalone `main`
 * feeding it pseudo-random byte buffers. Built under ASan/UBSan, any crash,
 * leak, overflow or UB inside the parser fails the run.
 *
 * With a libFuzzer-capable toolchain, compile with `-DSPARCLI_LIBFUZZER
 * -fsanitize=fuzzer,address` instead and the `main` below disappears.
 */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#ifndef SPARCLI_LIBFUZZER

/** Maximum fuzz input length per iteration (covers multi-line inputs). */
#define FUZZ_MAX_INPUT_LEN 512

/** xorshift64* PRNG: fast, deterministic, good enough for byte fuzzing. */
static uint64_t fuzz_rng_state;

static uint64_t fuzz_rand(void) {
    uint64_t x = fuzz_rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    fuzz_rng_state = x;
    return x * 0x2545f4914f6cdd1dULL;
}

/**
 * Fills `buf` with `len` random bytes, biased toward the interesting
 * alphabet of terminal parsers: ASCII, brackets, ESC, CSI bytes and raw
 * UTF-8 lead/continuation bytes.
 */
static void fuzz_fill(uint8_t *buf, size_t len) {
    static const uint8_t interesting[] = {
        0x1b, '[', ']', '/', ';', '~', 'm', 'A', 'O',
        0xc3, 0xe2, 0xf0, 0x80, 0xbf, 0x00, 0x7f, '\r', '\n',
    };
    for (size_t i = 0; i < len; i++) {
        uint64_t r = fuzz_rand();
        if ((r & 3) == 0) {
            buf[i] = interesting[(r >> 8) % sizeof interesting];
        } else {
            buf[i] = (uint8_t)(r >> 8);
        }
    }
}

int main(int argc, char *argv[]) {
    long iterations = argc > 1 ? strtol(argv[1], NULL, 10) : 100000;
    uint64_t seed = argc > 2 ? (uint64_t)strtoull(argv[2], NULL, 10) : 1;
    fuzz_rng_state = seed ? seed : 1;

    uint8_t buf[FUZZ_MAX_INPUT_LEN];
    for (long i = 0; i < iterations; i++) {
        size_t len = (size_t)(fuzz_rand() % (FUZZ_MAX_INPUT_LEN + 1));
        fuzz_fill(buf, len);
        LLVMFuzzerTestOneInput(buf, len);
    }
    printf("\033[32m✔\033[0m %s: %ld random inputs, no crash/leak/UB\n",
           argv[0], iterations);
    return 0;
}

#endif /* SPARCLI_LIBFUZZER */
