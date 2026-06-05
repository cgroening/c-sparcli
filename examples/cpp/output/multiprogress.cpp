// multiprogress.cpp - several progress bars updated together in place.
//
// Run (after `make`):
//   make run-example EX=cpp/output/multiprogress
//
// On a terminal the bars animate; piped/redirected, only the final stack is
// printed (the live display buffers off a TTY).

#include <sparcli.hpp>

#include <chrono>
#include <thread>

using namespace sparcli;


int main() {
    MultiProgress mp;

    ProgressBarOpts bo{ .type = SC_PROGRESS_BLOCK, .left_cap = "[",
                        .right_cap = "]" };
    bo.bar_width = 24;
    bo.show_percent = true;

    int download = mp.add("download", bo);
    int extract = mp.add("extract", bo);
    int verify = mp.add("verify", bo);

    for (int i = 0; i <= 100; ++i) {
        mp.update(download, i, 100);
        mp.update(extract, i * 0.7, 100);
        mp.update(verify, i * 0.4, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    mp.end();
    return 0;
}
