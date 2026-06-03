// progress_spinner.cpp - animated progress bars and spinners (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/progress_spinner

#include <sparcli.hpp>

#include <chrono>
#include <thread>

using namespace sparcli;


// Short pause between animation frames.
static void frame_delay(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

static void run_progress_bars();
static void run_spinner();


int main() {
    run_progress_bars();
    println("");
    run_spinner();
    return 0;
}

// A styled block bar and a threshold-colored line bar.
static void run_progress_bars() {
    ProgressBar download({ .type = SC_PROGRESS_BLOCK,
                           .fill_color = cyan(),
                           .show_value = true,
                           .width = 60, .label_width = 10,
                           .label_style = style(SC_TEXT_ATTR_BOLD) });
    download.set_label("download");
    for (int step = 0; step <= 200; ++step) {
        download.draw(step, 200);
        frame_delay(8);
    }
    download.finish(200, 200);

    ProgressBar deploy({ .type = SC_PROGRESS_LINE,
                         .thresholds = { .enabled = true,
                                         .mid = 0.4, .high = 0.8,
                                         .color_low = red(),
                                         .color_mid = yellow(),
                                         .color_high = green() },
                         .width = 60, .label_width = 10 });
    deploy.set_label("deploy");
    for (int step = 0; step <= 100; ++step) {
        // max == 0: the value is already a 0..1 ratio.
        deploy.draw(step / 100.0, 0);
        frame_delay(8);
    }
    deploy.finish(1.0, 0);
}

// A spinner whose label changes mid-animation.
static void run_spinner() {
    Spinner spinner("Connecting...", { .type = SC_SPINNER_BRAILLE,
                                       .color = cyan(),
                                       .label_style = style(SC_TEXT_ATTR_DIM) });
    for (int frame = 0; frame < 25; ++frame) {
        spinner.tick();
        frame_delay(40);
    }
    spinner.set_label("Fetching data...");
    for (int frame = 0; frame < 25; ++frame) {
        spinner.tick();
        frame_delay(40);
    }
    spinner.finish(true, "Done");
}
