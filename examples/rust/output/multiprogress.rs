//! Several progress bars updated together in place.
//!
//!   cargo run -p sparcli --example output_multiprogress
//!
//! On a terminal the bars animate; piped/redirected, only the final stack is
//! printed (the live display buffers off a TTY).

use std::thread::sleep;
use std::time::Duration;

use sparcli::*;

fn main() {
    let mut mp = MultiProgress::begin(MultiProgressOpts::new());

    let bar = || {
        ProgressBarOpts::new()
            .kind(ProgressType::Block)
            .show_percent(true)
    };
    let mut opts = bar();
    opts.bar_width = 24;
    opts.left_cap = Some("[".into());
    opts.right_cap = Some("]".into());

    let download = mp.add("download", opts.clone());
    let extract = mp.add("extract", opts.clone());
    let verify = mp.add("verify", opts);

    for i in 0..=100 {
        mp.update(download, i as f64, 100.0);
        mp.update(extract, i as f64 * 0.7, 100.0);
        mp.update(verify, i as f64 * 0.4, 100.0);
        sleep(Duration::from_millis(20));
    }

    mp.end();
}
