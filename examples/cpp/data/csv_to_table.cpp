// csv_to_table.cpp - read CSV with a header row and render it as a sparcli
// table (serde reader feeding the C++ output wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/data/csv_to_table

#include <sparcli.hpp>
#include <serde/sparcli_serde.hpp>

#include <cstddef>
#include <utility>
#include <vector>

using namespace sparcli;

int main() {
    serde::Csv doc = serde::Csv::parse(
        "name,role,commits\nAda,author,128\nLinus,maintainer,42\n",
        { .has_header = true }
    );

    Table table;
    for (std::size_t c = 0; c < doc.cols(); ++c) {
        table.add_column(doc.header(c));
    }
    for (std::size_t r = 0; r < doc.data_rows(); ++r) {
        std::vector<Table::Cell> row;
        for (std::size_t c = 0; c < doc.cols(); ++c) {
            row.emplace_back(doc.at(r + 1, c)); // +1 skips the header row
        }
        table.add_row(std::move(row));
    }

    table.print({
        .border = { .type = SC_BORDER_ROUNDED },
        .header = {
            .row = true,
            .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                       SC_ANSI_COLOR_NONE },
        },
    });
    return 0;
}
