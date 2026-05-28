#include "sparcli.h"
#include <stdio.h>


static const ScTextStyle plain = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle dim = {
    SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle keyword = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_green = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_red = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE
};


void test_lists(void) {
    printf(
        "\n\n══════════════════════════  LISTS  "
        "══════════════════════════\n\n"
    );

    /* ── 1. Bullet list (default •) ── */
    {
        printf("--- 1. Bullet list (default •) ---\n");
        ScList *list = sc_list_new((ScListOpts){
            .marker = SC_LIST_BULLET,
            .indent = 2,
            .item_gap = 0,
        });
        sc_list_add_str(list, "Install dependencies", plain);
        sc_list_add_str(list, "Configure the environment", plain);
        sc_list_add_str(list, "Run the build pipeline", plain);
        sc_list_add_str(list, "Deploy to production", plain);
        sc_list_print(list);
        sc_list_free(list);
    }

    /* ── 2. Custom bullet (→) with colored marker ── */
    {
        printf("\n--- 2. Custom bullet (→) with colored marker ---\n");
        ScList *list = sc_list_new((ScListOpts){
            .marker = SC_LIST_BULLET,
            .bullet = "\xe2\x86\x92",   /* → */
            .marker_style = bold_green,
            .indent = 2,
        });
        sc_list_add_str(list, "Frontend: React + TypeScript", plain);
        sc_list_add_str(list, "Backend: Go + PostgreSQL", plain);
        sc_list_add_str(list, "Infrastructure: Kubernetes on AWS", plain);
        sc_list_print(list);
        sc_list_free(list);
    }

    /* ── 3. Numbered list with word-wrap ── */
    {
        printf("\n--- 3. Numbered list with word-wrap ---\n");
        ScList *list = sc_list_new((ScListOpts){
            .marker = SC_LIST_NUMBER,
            .marker_suffix = ".",
            .indent = 2,
            .width = 60,
        });
        sc_list_add_str(list,
            "Read the full documentation before starting so that you "
            "understand all the configuration options available.",
            plain
        );
        sc_list_add_str(list,
            "Back up your existing configuration files, including any "
            "custom settings you have made to the environment.",
            plain
        );
        sc_list_add_str(list,
            "Run the migration script and verify the output carefully.",
            plain
        );
        sc_list_print(list);
        sc_list_free(list);
    }

    /* ── 4. Alpha lowercase with prefix/suffix ── */
    {
        printf("\n--- 4. Alpha lowercase (a) b) c)) ---\n");
        ScList *list = sc_list_new((ScListOpts){
            .marker = SC_LIST_ALPHA_LC,
            .marker_prefix = "",
            .marker_suffix = ")",
            .indent = 2,
        });
        sc_list_add_str(list, "Define the problem clearly", plain);
        sc_list_add_str(list, "Gather relevant data and context", plain);
        sc_list_add_str(list, "Propose at least three solutions", plain);
        sc_list_add_str(list, "Evaluate trade-offs for each approach", plain);
        sc_list_add_str(list, "Select and implement the best option", plain);
        sc_list_print(list);
        sc_list_free(list);
    }

    /* ── 5. Roman numeral uppercase ── */
    {
        printf("\n--- 5. Roman numerals uppercase (I. II. III.) ---\n");
        ScList *list = sc_list_new((ScListOpts){
            .marker = SC_LIST_ROMAN_UC,
            .marker_suffix = ".",
            .marker_style = bold,
            .indent = 2,
            .item_gap = 1,
        });
        sc_list_add_str(list, "Introduction", plain);
        sc_list_add_str(list, "Methodology", plain);
        sc_list_add_str(list, "Results", plain);
        sc_list_add_str(list, "Discussion", plain);
        sc_list_add_str(list, "Conclusion", plain);
        sc_list_add_str(list, "References", plain);
        sc_list_add_str(list, "Appendix", plain);
        sc_list_add_str(list, "Acknowledgements", plain);
        sc_list_print(list);
        sc_list_free(list);
    }

    /* ── 6. Nested lists ── */
    {
        printf("\n--- 6. Nested lists ---\n");
        ScList *list = sc_list_new((ScListOpts){
            .marker = SC_LIST_NUMBER,
            .marker_suffix = ".",
            .marker_style = bold,
            .indent = 2,
        });

        ScListItem *backend_item = sc_list_add_str(list, "Backend", plain);
        ScList *backend_sub = sc_list_add_sub(backend_item, (ScListOpts){
            .marker = SC_LIST_BULLET,
            .bullet = "\xe2\x80\x93",  /* – */
            .indent = 2,
        });
        sc_list_add_str(backend_sub, "REST API (Go)", plain);
        sc_list_add_str(backend_sub, "Database (Postgres)", plain);
        sc_list_add_str(backend_sub, "Cache (Redis)", plain);

        ScListItem *frontend_item = sc_list_add_str(list, "Frontend", plain);
        ScList *frontend_sub = sc_list_add_sub(frontend_item, (ScListOpts){
            .marker = SC_LIST_BULLET,
            .bullet = "\xe2\x80\x93",
            .indent = 2,
        });
        sc_list_add_str(frontend_sub, "React components", plain);
        sc_list_add_str(frontend_sub, "State management", plain);

        ScListItem *tooling_item = sc_list_add_str(
            frontend_sub, "Build tooling", plain
        );
        ScList *tooling_sub = sc_list_add_sub(tooling_item, (ScListOpts){
            .marker = SC_LIST_ALPHA_LC,
            .marker_suffix = ".",
            .indent = 2,
        });
        sc_list_add_str(tooling_sub, "Vite", plain);
        sc_list_add_str(tooling_sub, "ESLint", plain);
        sc_list_add_str(tooling_sub, "Prettier", plain);

        ScListItem *devops_item = sc_list_add_str(list, "DevOps", plain);
        ScList *devops_sub = sc_list_add_sub(devops_item, (ScListOpts){
            .marker = SC_LIST_BULLET,
            .bullet = "\xe2\x80\x93",
            .indent = 2,
        });
        sc_list_add_str(devops_sub, "Docker + Kubernetes", plain);
        sc_list_add_str(devops_sub, "GitHub Actions CI", plain);

        sc_list_print(list);
        sc_list_free(list);
    }

    /* ── 7. ScText items ── */
    {
        printf("\n--- 7. ScText items (mixed styles per item) ---\n");
        ScList *list = sc_list_new((ScListOpts){
            .marker = SC_LIST_BULLET,
            .indent = 2,
        });

        ScText *const_text = sc_text_new();
        sc_text_append(const_text, "Use ", plain);
        sc_text_append(const_text, "const", keyword);
        sc_text_append(const_text, " for values that never change", plain);

        ScText *types_text = sc_text_new();
        sc_text_append(types_text, "Prefer ", plain);
        sc_text_append(types_text, "explicit types", bold);
        sc_text_append(types_text, " over inference in public APIs", dim);

        ScText *make_text = sc_text_new();
        sc_text_append(make_text, "Run ", plain);
        sc_text_append(make_text, "make test", keyword);
        sc_text_append(make_text, " before every commit", plain);

        sc_list_add_text(list, const_text);
        sc_list_add_text(list, types_text);
        sc_list_add_text(list, make_text);

        sc_list_print(list);

        sc_list_free(list);
        sc_text_free(const_text);
        sc_text_free(types_text);
        sc_text_free(make_text);
    }

    /* ── 8. Two lists in columns ── */
    {
        printf("\n--- 8. Two lists in columns ---\n");
        ScList *pros = sc_list_new((ScListOpts){
            .marker = SC_LIST_BULLET,
            .bullet = "+",
            .marker_style = bold_green,
            .indent = 0,
        });
        sc_list_add_str(pros, "Fast compilation", plain);
        sc_list_add_str(pros, "Small binary size", plain);
        sc_list_add_str(pros, "Excellent performance", plain);
        sc_list_add_str(pros, "Strong type system", plain);

        ScList *cons = sc_list_new((ScListOpts){
            .marker = SC_LIST_BULLET,
            .bullet = "\xe2\x80\x93",
            .marker_style = bold_red,
            .indent = 0,
        });
        sc_list_add_str(cons, "Steep learning curve", plain);
        sc_list_add_str(cons, "Verbose error messages", plain);
        sc_list_add_str(cons, "Limited reflection", plain);

        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 4,
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_list(columns, pros, (ScColItem){ .min_w = 24 });
        sc_columns_add_list(columns, cons, (ScColItem){ .min_w = 24 });
        sc_columns_print(columns);

        sc_columns_free(columns);
        sc_list_free(pros);
        sc_list_free(cons);
    }
}
