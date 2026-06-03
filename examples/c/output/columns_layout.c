/*
 * columns_layout.c - side-by-side columns, capture/vstack composition,
 * padding and alignment helpers.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/columns_layout
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>


static void print_basic_columns(void);
static void print_composed_layout(void);
static void print_pad_and_align(void);
static void print_redirected(void);


int main(void) {
    print_basic_columns();
    printf("\n");
    print_composed_layout();
    printf("\n");
    print_pad_and_align();
    printf("\n");
    print_redirected();
    return 0;
}

/** Three columns with a separator, per-column width and alignment. */
static void print_basic_columns(void) {
    ScColumns *cols = sc_columns_new((ScColumnsOpts){
        .gap    = 2,
        .sep    = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_CYAN },
        .valign = SC_VALIGN_TOP,
    });

    sc_columns_add_str(cols, "Left column\nwith two lines.",
                       (ScColItem){ 0 });

    // A panel as a column; fixed width and middle vertical alignment.
    sc_columns_add_panel_str(cols, "Boxed middle column.",
        (ScPanelOpts){ .border = { .type = SC_BORDER_ROUNDED } },
        (ScColItem){ .fixed_w = 26, .valign_set = 1,
                     .valign = SC_VALIGN_MIDDLE });

    ScText *right = sc_markup_parse("[bold]Right[/]\n[dim]column[/]");
    sc_columns_add_text(cols, right, (ScColItem){ .halign = SC_ALIGN_RIGHT });

    sc_columns_print(cols);
    sc_text_free(right);
    sc_columns_free(cols);
}

/** Capture widgets, stack them vertically, place the stack in a column. */
static void print_composed_layout(void) {
    // Capture: render any widget into a reusable in-memory block.
    ScList *list = sc_list_new((ScListOpts){ .marker = SC_LIST_NUMBER });
    sc_list_add_str(list, "capture widgets", (ScTextStyle){ 0 });
    sc_list_add_str(list, "stack them with sc_vstack", (ScTextStyle){ 0 });
    sc_list_add_str(list, "drop the stack into a column", (ScTextStyle){ 0 });
    ScRendered *r_list = sc_capture_list(list);
    sc_list_free(list);

    ScRendered *r_rule = sc_capture_rule_str("composed", (ScRuleOpts){
        .type  = SC_BORDER_SINGLE,
        .width = 32,
    });

    // vstack: list above the rule, one blank line between them.
    const ScRendered *parts[] = { r_list, r_rule };
    ScRendered *stack = sc_vstack(parts, 2, 1);

    // The right column gets a panel; the left column gets the stack.
    ScColumns *cols = sc_columns_new((ScColumnsOpts){ .gap = 4 });
    sc_columns_add_rendered(cols, stack, (ScColItem){ 0 });
    sc_columns_add_panel_str(cols,
        "sc_columns copies captured\ncontent, so everything can\n"
        "be freed right after adding.",
        (ScPanelOpts){ .border = { .type = SC_BORDER_SINGLE } },
        (ScColItem){ 0 });
    sc_columns_print(cols);
    sc_columns_free(cols);

    sc_rendered_free(stack);
    sc_rendered_free(r_list);
    sc_rendered_free(r_rule);
}

/** Pad and align previously captured output. */
static void print_pad_and_align(void) {
    ScRendered *block = sc_capture_panel_str("padded + centered",
        (ScPanelOpts){ .border = { .type = SC_BORDER_THICK } });

    // Pad: blank lines on top, indentation on the left.
    sc_pad_print(block, (ScPadOpts){ .top = 1, .left = 4 });

    // Align: center the same block within the terminal width.
    sc_align_print(block, SC_ALIGN_CENTER, 0);

    sc_rendered_free(block);

    // One-step convenience variants for plain strings.
    sc_pad_str("indented by eight columns", (ScPadOpts){ .left = 8 });
    sc_align_str("centered heading", SC_ALIGN_CENTER, 0);
}

/** Redirect the output stream into a buffer instead of the terminal. */
static void print_redirected(void) {
    // sc_output_set_stream points rendering at any FILE* (thread-local), so
    // the same widgets compose with capture/pager. Here we capture into an
    // in-memory buffer and then inspect it.
    char *buffer = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buffer, &size);
    if (!mem) {
        return;
    }
    FILE *saved = sc_output_stream();
    sc_output_set_stream(mem);
    sc_println("rendered into a buffer", (ScTextStyle){ 0 });
    sc_output_set_stream(saved);   // always restore the previous stream
    fclose(mem);

    printf("redirected %zu bytes back to the terminal: %s",
           size, buffer ? buffer : "");
    free(buffer);
}
