// number_calc.cpp - numeric input, calculator mode, pure evaluator
// (C++ wrapper).
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/input/number_calc

#include <sparcli.hpp>

#include <print>

using namespace sparcli;


static void run_stepper();
static void run_calculator();
static void run_pure_evaluator();


int main() {
    if (!input_available()) {
        alert::warning("Run this example in a real terminal "
                       "(not under a pipe).");
        run_pure_evaluator();   // the evaluator is pure and needs no TTY
        return 0;
    }

    run_stepper();
    run_calculator();
    run_pure_evaluator();
    return 0;
}

// Up/Down steps the value; the result is clamped to [min, max].
static void run_stepper() {
    if (auto quantity = number_input("Quantity",
            { .initial = 1, .min = 1, .max = 99, .step = 1,
              .decimals = 0, .boxed = true, .width = 28 })) {
        std::println("  -> {:.0f}", *quantity);
    }
}

// Calculator mode: number_input_text returns the exact value as a string,
// '.'-normalized, so it can feed an arbitrary-precision decimal type.
static void run_calculator() {
    if (auto amount = number_input_text("Amount (try =1/3)",
            { .start_empty = true, .decimals = 2, .decimal_sep = ',',
              .calculator = true })) {
        std::println("  -> exact: {}", *amount);
    }
}

// calc_eval is the pure evaluator behind calculator mode.
static void run_pure_evaluator() {
    if (auto result = calc_eval("(2,5 + 1,5) * 4 - 1")) {
        std::println("  (2,5 + 1,5) * 4 - 1 = {}", *result);
    }
}
