#include "sparcli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void test_styles(void);
void test_colors(void);
void test_panels(void);
void test_tables(void);
void test_columns(void);
void test_rules(void);
void test_trees(void);
void test_lists(void);
void test_progressbar(void);
void test_progressbar_animated(void);
void test_spinner(void);
void test_spinner_animated(void);
void test_kv(void);
void test_alert(void);
void test_badge(void);
void test_util(void);
void test_pad(void);
void test_align(void);
void test_markup(void);


int main(void) {
    test_styles();
    test_colors();
    test_panels();
    test_tables();
    test_columns();
    test_rules();
    test_trees();
    test_lists();
    test_progressbar();
    // test_progressbar_animated();
    test_spinner();
    // test_spinner_animated();
    test_kv();
    test_alert();
    test_badge();
    test_util();
    test_pad();
    test_align();
    test_markup();
    return 0;
}
