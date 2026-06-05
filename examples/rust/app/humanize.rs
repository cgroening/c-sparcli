//! Human-readable sizes, durations, relative time and grouped numbers.
//!
//!   cargo run -p sparcli --example app_humanize

use sparcli::humanize::{self, ByteUnit, HumanizeOpts};

fn main() {
    println!("1536 (SI):   {}", humanize::bytes(1536, ByteUnit::Si));
    println!("1536 (IEC):  {}", humanize::bytes(1536, ByteUnit::Iec));
    println!("5 GB:        {}", humanize::bytes(5_000_000_000, ByteUnit::Si));

    let opts = HumanizeOpts::new();
    println!("grouped:     {}", humanize::number(1_234_567.0, opts.clone()));
    println!("compact:     {}", humanize::compact(12_400.0, opts.clone()));
    println!("percent:     {}", humanize::percent(0.42, opts));

    // German locale: comma decimal, dot grouping.
    let de = HumanizeOpts::new().decimals(2).decimal_sep(',').group_sep('.');
    println!("de_DE:       {}", humanize::number(1_234_567.89, de));

    println!("93s:         {}", humanize::duration(93.0));
    println!("8054s:       {}", humanize::duration(8054.0));
    println!("clock:       {}", humanize::duration_clock(3725.0));

    let now = 1_000_000_000_i64;
    println!("3h ago:      {}", humanize::relative(now - 3 * 3600, now));
    println!("in 2d:       {}", humanize::relative(now + 2 * 86_400, now));
}
