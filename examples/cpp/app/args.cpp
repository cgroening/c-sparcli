// args.cpp - declarative argument parser (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/app/args
// then try:
//   ./build.examples.nosync/cpp/app/args --help
//   ./build.examples.nosync/cpp/app/args convert in.png -f webp -q 90
//   ./build.examples.nosync/cpp/app/args convrt        (did-you-mean)

#include <sparcli.hpp>

#include <print>
#include <vector>

using namespace sparcli;


static Args build_parser();
static int run_convert(const Args& args);


int main(int argc, char** argv) {
    Args args = build_parser();

    // parse() returns the matched command (optional). On --help/--version
    // or an error it is empty; exit_code() distinguishes 0 from 2.
    auto matched = args.parse(argc, argv);
    if (!matched) {
        return args.exit_code();
    }

    // Each command carries its own handler (attached in build_parser), so the
    // matched node *is* the dispatch table: no name-compare switch needed.
    if (args.has_handler(*matched)) {
        return args.dispatch(*matched);
    }
    // Bare invocation (no subcommand): show the help screen.
    args.print_help();
    return 0;
}

// Declares the whole command-line interface via the chained builder. Each
// subcommand binds its action with .handler(...); the parser becomes the
// registry that maps the matched command straight to the code that runs it.
static Args build_parser() {
    Args args({ .prog = "imgtool", .version = "1.0.0",
                .about = "Example image conversion tool" });

    args.root().flag("verbose", 'v', "Explain what is being done");

    args.root().subcommand("convert", "Convert an image file")
        .opt("format", 'f', SC_ARG_STR, "FMT", "Output format")
        .opt_choices("format", { "png", "jpeg", "webp" })
        .opt_default("format", "png")
        .opt("quality", 'q', SC_ARG_INT, "N", "Quality (0-100)")
        .opt_default("quality", "80")
        .opt("tint", 0, SC_ARG_COLOR, "COLOR", "Tint color")
        .positional("INPUT", SC_ARG_STR, "Input file", true)
        .handler(run_convert);

    args.root().subcommand("completion", "Print the zsh completion script")
        .handler([](const Args& a) { a.print_zsh_completion(); return 0; });
    return args;
}

// The "convert" command: echo the parsed values.
static int run_convert(const Args& args) {
    std::println("input:   {}", args.get_str("INPUT").value_or(""));
    std::println("format:  {} (choice index {})",
                 args.get_str("format").value_or(""),
                 args.get_enum("format"));
    std::println("quality: {}", args.get_int("quality"));
    std::println("verbose: {}", args.get_flag("verbose") ? "yes" : "no");

    if (args.present("tint")) {
        println("tint:    this color",
                style(SC_TEXT_ATTR_BOLD, args.get_color("tint")));
    }

    alert::success("Conversion finished (pretend).");
    return 0;
}
