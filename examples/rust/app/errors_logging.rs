//! Structured error reports and leveled logging.
//!
//!   cargo run -p sparcli --example app_errors_logging

use sparcli::log;
use sparcli::*;

fn main() {
    show_error_report();
    show_global_logger();
    show_handle_logger();
}

/// Build an error with cause chain + hint and render it (without exiting).
fn show_error_report() {
    // A real tool would end with .die() (render to stderr + exit with code).
    ErrorReport::new("Configuration could not be loaded")
        .cause("file not found: ~/.config/app/config.toml")
        .cause("no such file or directory")
        .hint("Run 'app init' to create a default config.")
        .code(2)
        .print();
    println("", Style::default());
}

/// The global logger: stderr sink with a level filter.
fn show_global_logger() {
    log::set_level(log::LogLevel::Debug);

    log::debug("cache warmed");
    log::info("server listening on port 8080");
    log::warn("disk usage at 85 %");
    log::error("connection to db-01 lost");

    log::reset(); // restore defaults for any code that runs afterwards
}

/// A handle-based logger writing to a file (closed on drop).
fn show_handle_logger() {
    if let Some(path) = paths::file(paths::Kind::State, "sparcli-log-example", "debug.log") {
        let logger = Logger::new(LoggerOpts::new().hide_timestamps())
            .add_file(&path.display().to_string(), log::LogLevel::Debug);
        logger.info("deployment succeeded");
        logger.warn("rollback plan not configured");
        println(
            &format!("Wrote 2 records to {}", path.display()),
            Style::dim(),
        );
    }
}
