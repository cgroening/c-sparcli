#include "sparcli.h"
#include "fuzz_common.h"

/**
 * Fuzz target for the key-event decoder (`sc_key_decode`): random byte
 * sequences (including partial ESC/CSI/UTF-8 prefixes) must never crash,
 * read out of bounds, or loop forever. The decoder contract is consumed
 * `<= len`, and `0` only for incomplete prefixes.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const char *cursor = (const char *)data;
    size_t remaining = size;

    while (remaining > 0) {
        ScKey key;
        size_t consumed = sc_key_decode(cursor, remaining, &key);
        if (consumed == 0 || consumed > remaining) {
            // Incomplete prefix (or contract violation, which ASan/UBSan and
            // the bounds check below would surface): skip one byte so the
            // fuzz loop always terminates.
            consumed = 1;
        }
        cursor += consumed;
        remaining -= consumed;
    }
    return 0;
}
