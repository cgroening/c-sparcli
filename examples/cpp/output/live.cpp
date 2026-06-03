// live.cpp - live display: re-render a composed frame in place (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/live
//
// Off a TTY (piped) only the final frame is printed.

#include <sparcli.hpp>

#include <chrono>
#include <format>
#include <thread>

using namespace sparcli;


// Total animation steps (kept low so the demo finishes quickly).
constexpr int kTotalSteps = 30;


static Rendered build_frame(int step);


int main() {
    markup::println("[bold]Live dashboard[/] [dim](redraws in place)[/]\n");

    {
        // RAII: the Live session restores the terminal on scope exit.
        Live live;
        for (int step = 0; step <= kTotalSteps; ++step) {
            live.update(build_frame(step));
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
        }
    }

    markup::println("\n[green]✔[/] Frame above was redrawn "
                    "in place on every update.");
    return 0;
}

// Composes one dashboard frame: a status table plus a progress line.
static Rendered build_frame(int step) {
    int percent = step * 100 / kTotalSteps;

    Table table;
    table.add_column("Job", { .min_width = 10 });
    table.add_column("Status", { .min_width = 14 });
    table.add_row({ "build",
                    percent >= 50 ? "done" : "running" });
    table.add_row({ "package",
                    percent >= 100 ? "done"
                    : percent >= 50 ? "running" : "waiting" });

    Rendered table_part = capture::table(
        table, { .border = { .type = SC_BORDER_ROUNDED, .outer_color = cyan() },
                 .header = { .row = true, .style = style(SC_TEXT_ATTR_BOLD) } });
    Rendered line_part = capture::str(
        std::format("progress: {} %", percent));

    return vstack({ &table_part, &line_part }, 1);
}
