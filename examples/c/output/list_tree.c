/*
 * list_tree.c - bulleted/numbered lists with nesting, and tree views.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/list_tree
 */

#include <sparcli.h>

#include <stdio.h>


static void print_lists(void);
static void print_tree(void);


int main(void) {
    print_lists();
    printf("\n");
    print_tree();
    return 0;
}

/** Markers, styling, nesting and word-wrap in lists. */
static void print_lists(void) {
    // Numbered list with a styled marker and a nested sub-list.
    ScList *steps = sc_list_new((ScListOpts){
        .marker       = SC_LIST_NUMBER,
        .marker_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                          SC_ANSI_COLOR_NONE },
    });
    sc_list_add_str(steps, "Check out the repository", (ScTextStyle){ 0 });
    ScListItem *build = sc_list_add_str(steps, "Build the project",
                                        (ScTextStyle){ 0 });
    sc_list_add_str(steps, "Run the test suite", (ScTextStyle){ 0 });

    // Sub-list: attached to an item, freed together with the parent list.
    ScList *sub = sc_list_add_sub(build, (ScListOpts){
        .marker = SC_LIST_ALPHA_LC,
        .indent = 2,
    });
    sc_list_add_str(sub, "make", (ScTextStyle){ 0 });
    sc_list_add_str(sub, "make qa", (ScTextStyle){ 0 });

    sc_list_print(steps);
    sc_list_free(steps);
    printf("\n");

    // Bullet list with a custom bullet, rich-text items and word-wrap.
    ScList *notes = sc_list_new((ScListOpts){
        .marker = SC_LIST_BULLET,
        .bullet = "→",
        .width  = 46,    // wrap long items; continuation lines hang-indent
    });
    sc_list_add_str(notes,
                    "Long items are word-wrapped and continuation lines are "
                    "indented under the first word of the item.",
                    (ScTextStyle){ 0 });

    ScText *rich = sc_markup_parse("Items can be [bold green]rich text[/].");
    sc_list_add_text(notes, rich);
    sc_list_print(notes);
    sc_text_free(rich);
    sc_list_free(notes);
}

/** A file-system-like tree with prefixes and colored connectors. */
static void print_tree(void) {
    ScTree *tree = sc_tree_new((ScTreeOpts){
        .type            = SC_BORDER_SINGLE,
        .connector_color = SC_ANSI_COLOR_CYAN,
    });

    ScTextStyle dir_style  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLUE,
                               SC_ANSI_COLOR_NONE };
    ScTextStyle file_style = { 0 };

    // parent = NULL adds a root node.
    ScTreeNode *root = sc_tree_add_str(tree, NULL, "sparcli/", dir_style,
                                       NULL, (ScTextStyle){ 0 });

    ScTreeNode *src = sc_tree_add_str(tree, root, "src/", dir_style,
                                      NULL, (ScTextStyle){ 0 });
    sc_tree_add_str(tree, src, "core/", dir_style, NULL, (ScTextStyle){ 0 });
    sc_tree_add_str(tree, src, "output/", dir_style, NULL, (ScTextStyle){ 0 });

    ScTreeNode *docs = sc_tree_add_str(tree, root, "docs/", dir_style,
                                       NULL, (ScTextStyle){ 0 });
    // Prefix: a marker rendered before the node text, with its own style.
    sc_tree_add_str(tree, docs, "examples.md", file_style,
                    "▪ ", (ScTextStyle){ .fg = SC_ANSI_COLOR_GREEN });

    sc_tree_add_str(tree, root, "Makefile", file_style,
                    NULL, (ScTextStyle){ 0 });

    sc_tree_print(tree);
    sc_tree_free(tree);
}
