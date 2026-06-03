// columns_layout.cpp - side-by-side columns, capture/vstack, pad/align
// (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/columns_layout

#include <sparcli.hpp>

using namespace sparcli;


static void print_basic_columns();
static void print_composed_layout();
static void print_pad_and_align();


int main() {
    print_basic_columns();
    println("");
    print_composed_layout();
    println("");
    print_pad_and_align();
    return 0;
}

// Columns with a separator and per-column width/alignment.
static void print_basic_columns() {
    Columns cols({ .gap = 2,
                   .sep = { .type = SC_BORDER_SINGLE, .color = cyan() },
                   .valign = SC_VALIGN_TOP });

    cols.add(std::string_view("Left column\nwith two lines."), {});
    cols.add_panel("Boxed middle column.",
                   { .border = { .type = SC_BORDER_ROUNDED } },
                   { .fixed_w = 26, .valign_set = 1,
                     .valign = SC_VALIGN_MIDDLE });

    Text right = markup::parse("[bold]Right[/]\n[dim]column[/]");
    cols.add(right, { .halign = SC_ALIGN_RIGHT });
    cols.print();
}

// Capture widgets, stack them vertically, place the stack in a column.
static void print_composed_layout() {
    List list({ .marker = SC_LIST_NUMBER });
    list.add("capture widgets");
    list.add("stack them with vstack");
    list.add("drop the stack into a column");

    Rendered r_list = capture::list(list);
    Rendered r_rule = capture::rule("composed", { .type = SC_BORDER_SINGLE,
                                                  .width = 32 });
    Rendered stack  = vstack({ &r_list, &r_rule }, 1);

    Columns cols({ .gap = 4 });
    cols.add(stack);
    cols.add_panel("Columns copy captured\ncontent, so the sources\n"
                   "can be freed right away.",
                   { .border = { .type = SC_BORDER_SINGLE } });
    cols.print();
}

// Pad and align a captured block, then the one-step string helpers.
static void print_pad_and_align() {
    Rendered block = capture::panel("padded + centered",
                                    { .border = { .type = SC_BORDER_THICK } });
    block.pad({ .top = 1, .left = 4 });
    block.align(SC_ALIGN_CENTER);   // 0 width = terminal width

    pad("indented by eight columns", { .left = 8 });
    align("centered heading", SC_ALIGN_CENTER, 0);
}
