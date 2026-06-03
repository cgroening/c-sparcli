// errors_logging.cpp - structured error reports and leveled logging
// (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/app/errors_logging

#include <sparcli.hpp>

#include <cstdio>

using namespace sparcli;


static void show_error_report();
static void show_global_logger();
static void show_handle_logger();


int main() {
    show_error_report();
    show_global_logger();
    show_handle_logger();
    return 0;
}

// Build an error with cause chain + hint and render it (without exiting).
static void show_error_report() {
    // In a real tool this would end with .die() (render to stderr + exit).
    ErrorReport report("Configuration could not be loaded");
    report.cause("file not found: ~/.config/app/config.toml")
          .cause("no such file or directory")
          .hint("Run 'app init' to create a default config.")
          .code(2);
    report.print();   // render to the output stream, no exit
    println("");
}

// The global logger: stderr sink with a level filter.
static void show_global_logger() {
    logging::set_level(SC_LOG_DEBUG);
    // logging::add_file("app.log", SC_LOG_DEBUG);   // optional file sink

    logging::debug("cache warmed");
    logging::info("server listening on port 8080");
    logging::warn("disk usage at 85 %");
    logging::error("connection to db-01 lost");

    logging::reset();   // restore defaults for code that runs afterwards
    println("");
}

// A handle-based logger: own sinks and options (RAII), independent of global.
static void show_handle_logger() {
    Logger logger(LoggerOpts{ .hide_timestamps = true,
                              .show_source = true,
                              .markup = true });
    logger.add_terminal(stderr, SC_LOG_DEBUG);
    // logger.add_file("debug.log");

    // Messages are passed as data (never as a format string).
    logger.info("deployment [bold green]succeeded[/]");
    logger.warn("rollback plan not configured");
}
