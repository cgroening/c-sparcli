//! Colored unified diff of two texts.
//!
//!   cargo run -p sparcli --example output_diff

use sparcli::*;

fn main() {
    let before = "name = sparcli\nversion = 0.1.0\ncolor = blue\n";
    let after = "name = sparcli\nversion = 0.2.0\ncolor = green\ndebug = true\n";

    diff_print(
        before,
        after,
        DiffOpts::new()
            .old_label("config.toml (old)")
            .new_label("config.toml (new)")
            .context(1),
    );

    println("", Style::default());

    // The same diff captured and framed with padding.
    let r = capture::diff(before, after, DiffOpts::new().no_header(true));
    r.pad(PadOpts { left: 2, ..Default::default() });
}
