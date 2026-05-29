#include "test_input.h"
#include "sparcli.h"


/** Pure unit tests for the built-in character filters. */
void test_filters(void) {
    CHECK(sc_filter_digits('5', NULL) && !sc_filter_digits('a', NULL),
          "digits: accepts 0-9, rejects letters");
    CHECK(sc_filter_decimal('.', NULL) && sc_filter_decimal('-', NULL)
          && sc_filter_decimal('7', NULL) && !sc_filter_decimal('x', NULL),
          "decimal: accepts digits/sign/point");
    CHECK(sc_filter_alpha('A', NULL) && sc_filter_alpha('z', NULL)
          && !sc_filter_alpha('1', NULL),
          "alpha: letters only");
    CHECK(sc_filter_alnum('Z', NULL) && sc_filter_alnum('9', NULL)
          && !sc_filter_alnum('-', NULL),
          "alnum: letters and digits");
    CHECK(!sc_filter_no_space(' ', NULL) && !sc_filter_no_space('\t', NULL)
          && sc_filter_no_space('x', NULL),
          "no_space: rejects whitespace");
}
