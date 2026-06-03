// pager.cpp - route long output through $PAGER / `less -R` (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/pager
//
// In a terminal the table opens in the pager (press `q`). Off a TTY the
// pager is skipped and the output prints directly.

#include <sparcli.hpp>

#include <format>
#include <print>
#include <string>

using namespace sparcli;


// Number of generated rows; enough to make paging worthwhile.
constexpr int kRowCount = 100;


int main() {
    int status = 0;
    {
        // RAII pager: everything in this scope is paged; end on scope exit.
        Pager pager;

        rule("100 rows", { .type = SC_BORDER_DOUBLE });

        Table table;
        table.add_column("#", { .halign = SC_ALIGN_RIGHT });
        table.add_column("Square", { .halign = SC_ALIGN_RIGHT });
        for (int i = 1; i <= kRowCount; ++i) {
            // The wrapper copies cell strings, so temporaries are fine.
            table.add_row({ std::to_string(i), std::to_string(i * i) });
        }
        table.print({ .border = { .type = SC_BORDER_SINGLE },
                      .header = { .row = true,
                                  .style = style(SC_TEXT_ATTR_BOLD) } });

        status = pager.end();
    }

    std::println("pager exit status: {}", status);
    return 0;
}
