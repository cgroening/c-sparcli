use std::path::PathBuf;

/// C sources, mirroring the `SRC` list in the repo Makefile. Paths are relative
/// to the repo root.
const SOURCES: &[&str] = &[
    "src/core/output.c",
    "src/core/version.c",
    "src/core/text_attributes.c",
    "src/core/print.c",
    "src/core/color.c",
    "src/core/text.c",
    "src/core/render_wrap.c",
    "src/output/panel.c",
    "src/output/table/table.c",
    "src/output/table/table_print.c",
    "src/output/table/table_print_init.c",
    "src/output/table/table_print_render.c",
    "src/output/table/table_print_render_cell.c",
    "src/output/table/table_print_render_border.c",
    "src/output/table/table_print_render_row.c",
    "src/output/columns.c",
    "src/output/rule.c",
    "src/output/tree.c",
    "src/output/list.c",
    "src/output/progressbar.c",
    "src/output/spinner.c",
    "src/output/kv.c",
    "src/output/alert.c",
    "src/output/badge.c",
    "src/output/util.c",
    "src/output/pad.c",
    "src/output/markup.c",
    "src/tty/term.c",
    "src/tty/key.c",
    "src/tty/screen.c",
    "src/input/prompt.c",
    "src/input/line_editor.c",
    "src/input/theme.c",
    "src/input/shortcut.c",
    "src/input/editor.c",
    "src/input/confirm.c",
    "src/input/text_input.c",
    "src/input/password_input.c",
    "src/input/number_input.c",
    "src/input/textarea.c",
    "src/input/select.c",
    "src/input/fuzzy.c",
    "src/input/datepicker.c",
];

fn main() {
    // bindings/rust/sparcli-sys -> repo root is three levels up.
    let manifest = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let root = manifest.join("../../..").canonicalize().unwrap();
    let include = root.join("include");
    let src = root.join("src");

    let mut build = cc::Build::new();
    build.std("c11").include(&include).include(&src).warnings(false);
    for s in SOURCES {
        let p = root.join(s);
        println!("cargo:rerun-if-changed={}", p.display());
        build.file(p);
    }
    println!("cargo:rerun-if-changed={}", include.display());
    build.compile("sparcli");

    #[cfg(feature = "regen")]
    {
        let out = PathBuf::from(std::env::var("OUT_DIR").unwrap());
        let bindings = bindgen::Builder::default()
            .header(include.join("sparcli.h").to_string_lossy())
            .clang_arg(format!("-I{}", include.display()))
            .allowlist_function("sc_.*")
            .allowlist_type("Sc.*")
            .allowlist_var("SC_.*")
            .generate()
            .expect("bindgen failed");
        bindings
            .write_to_file(out.join("bindings.rs"))
            .expect("write bindings failed");
    }
}
