// datepicker.cpp - calendar month grid for picking a date (C++ wrapper).
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/input/datepicker

#include <sparcli.hpp>

#include <array>
#include <ctime>
#include <print>

using namespace sparcli;


// Formats a std::tm as "Weekday, DD Month YYYY".
static std::string format_date(const std::tm& date, const char* pattern) {
    std::array<char, 64> buffer{};
    std::tm copy = date;
    std::strftime(buffer.data(), buffer.size(), pattern, &copy);
    return buffer.data();
}


int main() {
    if (!input_available()) {
        alert::warning("Run this example in a real terminal "
                       "(not under a pipe).");
        return 0;
    }

    // A default-constructed std::tm ({}) seeds the picker with today.
    if (auto picked = datepicker({}, { .prompt = "Delivery date",
                                       .week_start = SC_WEEK_START_MONDAY,
                                       .accent = cyan() })) {
        std::println("  -> {}", format_date(*picked, "%A, %d %B %Y"));
    }

    // Pre-seed a specific start date.
    std::tm start{};
    start.tm_year = 2027 - 1900;   // years since 1900
    start.tm_mon  = 0;             // January
    start.tm_mday = 1;
    if (auto picked = datepicker(start,
            { .prompt = "Starts at 2027-01-01 (Sunday weeks)",
              .week_start = SC_WEEK_START_SUNDAY })) {
        std::println("  -> {}", format_date(*picked, "%Y-%m-%d"));
    }

    return 0;
}
