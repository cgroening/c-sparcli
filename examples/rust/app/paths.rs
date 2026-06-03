//! Per-application XDG directories and files.
//!
//!   cargo run -p sparcli --example app_paths

use sparcli::paths;
use sparcli::*;

const APP_NAME: &str = "sparcli-paths-example";

fn main() {
    // Each returns Option<PathBuf>; the dirs are created (0700) on first use.
    let (Some(config), Some(data), Some(cache), Some(state)) = (
        paths::config(APP_NAME),
        paths::data(APP_NAME),
        paths::cache(APP_NAME),
        paths::state(APP_NAME),
    ) else {
        alert::error("Could not resolve the XDG directories.");
        return;
    };

    let mut dirs = Kv::new(KvOpts::new().key_style(Style::bold().fg(Color::CYAN)));
    dirs.add("Config", &config.display().to_string())
        .add("Data", &data.display().to_string())
        .add("Cache", &cache.display().to_string())
        .add("State", &state.display().to_string());
    dirs.print();
    println("", Style::default());

    // A file path inside an app dir: parents created, file not.
    if let Some(log_file) = paths::file(paths::Kind::State, APP_NAME, "logs/run.log") {
        println(
            &format!("Log file would go to: {}", log_file.display()),
            Style::default(),
        );
    }
}
