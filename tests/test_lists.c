#include "sparcli.h"
#include <stdio.h>


void test_lists(void) {
    printf("\n\n══════════════════════════  LISTS  ══════════════════════════\n\n");

    /* ── 1. Bullet list (default •) ── */
    {
        printf("--- 1. Bullet list (default •) ---\n");
        ScList *l = sc_list_new((ScListOpts){
            .marker   = SC_LIST_BULLET,
            .indent   = 2,
            .item_gap = 0,
        });
        ScTextStyle none = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        sc_list_add_str(l, "Install dependencies", none);
        sc_list_add_str(l, "Configure the environment", none);
        sc_list_add_str(l, "Run the build pipeline", none);
        sc_list_add_str(l, "Deploy to production", none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 2. Custom bullet (→) with colored marker ── */
    {
        printf("\n--- 2. Custom bullet (→) with colored marker ---\n");
        ScTextStyle bold_green = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE };
        ScTextStyle none       = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker      = SC_LIST_BULLET,
            .bullet      = "\xe2\x86\x92",   /* → */
            .marker_opts = bold_green,
            .indent      = 2,
        });
        sc_list_add_str(l, "Frontend: React + TypeScript", none);
        sc_list_add_str(l, "Backend: Go + PostgreSQL", none);
        sc_list_add_str(l, "Infrastructure: Kubernetes on AWS", none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 3. Numbered list with word-wrap ── */
    {
        printf("\n--- 3. Numbered list with word-wrap ---\n");
        ScTextStyle none = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker        = SC_LIST_NUMBER,
            .marker_suffix = ".",
            .indent        = 2,
            .width         = 60,
        });
        sc_list_add_str(l,
            "Read the full documentation before starting so that you understand "
            "all the configuration options available.", none);
        sc_list_add_str(l,
            "Back up your existing configuration files, including any custom "
            "settings you have made to the environment.", none);
        sc_list_add_str(l,
            "Run the migration script and verify the output carefully.", none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 4. Alpha lowercase with prefix/suffix ── */
    {
        printf("\n--- 4. Alpha lowercase (a) b) c)) ---\n");
        ScTextStyle none = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker        = SC_LIST_ALPHA_LC,
            .marker_prefix = "",
            .marker_suffix = ")",
            .indent        = 2,
        });
        sc_list_add_str(l, "Define the problem clearly", none);
        sc_list_add_str(l, "Gather relevant data and context", none);
        sc_list_add_str(l, "Propose at least three solutions", none);
        sc_list_add_str(l, "Evaluate trade-offs for each approach", none);
        sc_list_add_str(l, "Select and implement the best option", none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 5. Roman numeral uppercase ── */
    {
        printf("\n--- 5. Roman numerals uppercase (I. II. III.) ---\n");
        ScTextStyle bold = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScTextStyle none = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker        = SC_LIST_ROMAN_UC,
            .marker_suffix = ".",
            .marker_opts   = bold,
            .indent        = 2,
            .item_gap      = 1,
        });
        sc_list_add_str(l, "Introduction",      none);
        sc_list_add_str(l, "Methodology",       none);
        sc_list_add_str(l, "Results",           none);
        sc_list_add_str(l, "Discussion",        none);
        sc_list_add_str(l, "Conclusion",        none);
        sc_list_add_str(l, "References",        none);
        sc_list_add_str(l, "Appendix",          none);
        sc_list_add_str(l, "Acknowledgements",  none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 6. Nested lists ── */
    {
        printf("\n--- 6. Nested lists ---\n");
        ScTextStyle none  = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScTextStyle bold  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker   = SC_LIST_NUMBER,
            .marker_suffix = ".",
            .marker_opts   = bold,
            .indent   = 2,
        });

        ScListItem *it1 = sc_list_add_str(l, "Backend", none);
        ScList *sub1 = sc_list_add_sub(it1, (ScListOpts){
            .marker  = SC_LIST_BULLET,
            .bullet  = "\xe2\x80\x93",  /* – */
            .indent  = 2,
        });
        sc_list_add_str(sub1, "REST API (Go)",       none);
        sc_list_add_str(sub1, "Database (Postgres)", none);
        sc_list_add_str(sub1, "Cache (Redis)",       none);

        ScListItem *it2 = sc_list_add_str(l, "Frontend", none);
        ScList *sub2 = sc_list_add_sub(it2, (ScListOpts){
            .marker  = SC_LIST_BULLET,
            .bullet  = "\xe2\x80\x93",
            .indent  = 2,
        });
        sc_list_add_str(sub2, "React components",    none);
        sc_list_add_str(sub2, "State management",    none);

        ScListItem *it2b = sc_list_add_str(sub2, "Build tooling", none);
        ScList *sub2b = sc_list_add_sub(it2b, (ScListOpts){
            .marker  = SC_LIST_ALPHA_LC,
            .marker_suffix = ".",
            .indent  = 2,
        });
        sc_list_add_str(sub2b, "Vite",    none);
        sc_list_add_str(sub2b, "ESLint",  none);
        sc_list_add_str(sub2b, "Prettier",none);

        ScListItem *it3 = sc_list_add_str(l, "DevOps", none);
        ScList *sub3 = sc_list_add_sub(it3, (ScListOpts){
            .marker  = SC_LIST_BULLET,
            .bullet  = "\xe2\x80\x93",
            .indent  = 2,
        });
        sc_list_add_str(sub3, "Docker + Kubernetes", none);
        sc_list_add_str(sub3, "GitHub Actions CI",   none);

        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 7. ScText items ── */
    {
        printf("\n--- 7. ScText items (mixed styles per item) ---\n");
        ScTextStyle none    = { SC_TEXT_ATTR_NONE,   SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };
        ScTextStyle bold    = { SC_TEXT_ATTR_BOLD,   SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };
        ScTextStyle dim     = { SC_TEXT_ATTR_DIM,    SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };
        ScTextStyle keyword = { SC_TEXT_ATTR_BOLD,   SC_ANSI_COLOR_CYAN,  SC_ANSI_COLOR_NONE };

        ScList *l = sc_list_new((ScListOpts){
            .marker   = SC_LIST_BULLET,
            .indent   = 2,
        });

        ScText *t1 = sc_text_new();
        sc_text_append(t1, "Use ", none);
        sc_text_append(t1, "const", keyword);
        sc_text_append(t1, " for values that never change", none);

        ScText *t2 = sc_text_new();
        sc_text_append(t2, "Prefer ", none);
        sc_text_append(t2, "explicit types", bold);
        sc_text_append(t2, " over inference in public APIs", dim);

        ScText *t3 = sc_text_new();
        sc_text_append(t3, "Run ", none);
        sc_text_append(t3, "make test", keyword);
        sc_text_append(t3, " before every commit", none);

        sc_list_add_text(l, t1);
        sc_list_add_text(l, t2);
        sc_list_add_text(l, t3);

        sc_list_print(l);

        sc_list_free(l);
        sc_text_free(t1);
        sc_text_free(t2);
        sc_text_free(t3);
    }

    /* ── 8. Two lists in columns ── */
    {
        printf("\n--- 8. Two lists in columns ---\n");
        ScTextStyle none = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScTextStyle bold = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };

        ScList *pros = sc_list_new((ScListOpts){
            .marker      = SC_LIST_BULLET,
            .bullet      = "+",
            .marker_opts = (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE },
            .indent      = 0,
        });
        sc_list_add_str(pros, "Fast compilation",          none);
        sc_list_add_str(pros, "Small binary size",         none);
        sc_list_add_str(pros, "Excellent performance",     none);
        sc_list_add_str(pros, "Strong type system",        none);

        ScList *cons = sc_list_new((ScListOpts){
            .marker      = SC_LIST_BULLET,
            .bullet      = "\xe2\x80\x93",
            .marker_opts = (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE },
            .indent      = 0,
        });
        sc_list_add_str(cons, "Steep learning curve",      none);
        sc_list_add_str(cons, "Verbose error messages",    none);
        sc_list_add_str(cons, "Limited reflection",        none);

        ScText *th_pros = sc_text_new();
        sc_text_append(th_pros, "Pros", bold);
        ScText *th_cons = sc_text_new();
        sc_text_append(th_cons, "Cons", bold);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 4,
            .sep = { .style = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .valign    = SC_VALIGN_TOP,
        });
        sc_columns_add_list(cl, pros, (ScColItem){ .min_w = 24 });
        sc_columns_add_list(cl, cons, (ScColItem){ .min_w = 24 });
        sc_columns_print(cl);

        sc_columns_free(cl);
        sc_list_free(pros);
        sc_list_free(cons);
        sc_text_free(th_pros);
        sc_text_free(th_cons);
    }
}
