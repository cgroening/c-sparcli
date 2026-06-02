# sparcli – Framework API Reference (C)

The application-framework side of the C library: everything that supports the *application around the widgets* – where its files live, how it logs, how it reports errors, and how it parses its command line. The terminal/widget API (panels, tables, prompts, markup, …) is documented in [`api-c.md`](api-c.md).

All functions follow the library conventions: `sc_` prefix, zero-init-friendly opts structs, heap results paired with `free()`, and `SPARCLI_EXPORT` visibility. Headers live under `include/{app,log,args}/` and are included by the `sparcli.h` umbrella.

## Table of contents

- [XDG Paths](#xdg-paths)
- [Pretty Errors](#pretty-errors)
- [Logging](#logging)
- [Argument Parser](#argument-parser)

---

## XDG Paths

Resolve (and create) the standard per-user directories where a CLI application keeps its files, per the [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/latest/). Header: `app/sparcli_paths.h`.

| Kind | Environment variable | Fallback | Typical content |
|------|-----------------------|----------|-----------------|
| `SC_PATH_CONFIG` | `$XDG_CONFIG_HOME` | `~/.config` | Configuration files |
| `SC_PATH_DATA` | `$XDG_DATA_HOME` | `~/.local/share` | Persistent data |
| `SC_PATH_CACHE` | `$XDG_CACHE_HOME` | `~/.cache` | Re-creatable caches |
| `SC_PATH_STATE` | `$XDG_STATE_HOME` | `~/.local/state` | Logs, history, state |

### Functions

```c
char *sc_path(ScPathKind kind, const char *appname);
char *sc_path_config(const char *appname);   /* sc_path(SC_PATH_CONFIG, …) */
char *sc_path_data  (const char *appname);   /* sc_path(SC_PATH_DATA, …)   */
char *sc_path_cache (const char *appname);   /* sc_path(SC_PATH_CACHE, …)  */
char *sc_path_state (const char *appname);   /* sc_path(SC_PATH_STATE, …)  */

char *sc_path_file(ScPathKind kind, const char *appname, const char *relative);
```

Every function returns a **heap-allocated absolute path** (caller must `free()`) or `NULL` on failure. The directory is created on first use (mode `0700`, including parents), so the caller can write into it immediately.

```c
/* ~/.config/myapp (created if missing) */
char *config_dir = sc_path_config("myapp");

/* ~/.local/state/myapp/logs/run.log – parents created, file not created */
char *log_path = sc_path_file(SC_PATH_STATE, "myapp", "logs/run.log");

free(config_dir);
free(log_path);
```

### Behavior & safety

- **Environment override:** a `$XDG_*` variable wins only when it holds an **absolute** path; relative values are ignored per the XDG spec (falls back to `$HOME`).
- **`appname` validation:** must be a single path component – names containing `/`, `.`/`..` or control characters return `NULL`.
- **`relative` validation (`sc_path_file`):** may contain subdirectories but must stay inside the application directory – absolute paths, `..` segments, empty components (`//`) and control characters return `NULL` (path-traversal guard).
- **No `$HOME`:** when neither the `$XDG_*` variable nor a usable `$HOME` exists, the functions return `NULL`.
- `sc_path_file` creates the parent directories of the file, never the file itself.

### Typical pattern

```c
#include <sparcli.h>

int main(void) {
    char *log_path = sc_path_file(SC_PATH_STATE, "myapp", "myapp.log");
    if (!log_path) {
        fprintf(stderr, "myapp: cannot resolve state directory\n");
        return 1;
    }
    FILE *log_file = fopen(log_path, "a");
    /* … */
    free(log_path);
}
```

### Bindings

| Language | API |
|----------|-----|
| C++ | `sparcli::paths::config/data/cache/state(app)` → `std::optional<std::string>`, `paths::file(kind, app, rel)`, `paths::dir(kind, app)` |
| Rust | `sparcli::paths::config/data/cache/state(app)` → `Option<PathBuf>`, `paths::file(kind, app, rel)`, `paths::Kind` |
| Python | `sc.config_dir/data_dir/cache_dir/state_dir(app)` → `pathlib.Path` (raises `SparcliError`), `sc.app_file(kind, app, rel)`, `sc.PathKind` |

---

## Pretty Errors

Structured, panel-rendered error reporting: the replacement for `fprintf(stderr, "error: …"); exit(1);`. An error carries a **message**, an optional **cause chain**, an optional **hint** and an **exit code**, and renders as a red alert panel. Header: `app/sparcli_error.h`.

```
╭─ ✖ Error ────────────────────────────────────────────────────────────────────╮
│ Config could not be loaded                                                   │
│   caused by: file not found: ~/.config/app/config.toml                       │
│                                                                              │
│ Hint: Run 'app init' to create a default config                              │
╰──────────────────────────────────────────────────────────────────────────────╯
```

### Functions

```c
ScError *sc_error_new(const char *message);                    /* builder; strings copied */
void     sc_error_add_cause(ScError *e, const char *cause);    /* repeatable */
void     sc_error_set_hint(ScError *e, const char *hint);
void     sc_error_set_code(ScError *e, int exit_code);         /* default 1 */
int      sc_error_code(const ScError *e);

void     sc_error_print(const ScError *e);          /* render to output stream; no exit */
void     sc_error_print_stderr(const ScError *e);   /* render to stderr; no exit */
void     sc_error_free(ScError *e);

_Noreturn void sc_die(ScError *e);                  /* render to stderr + free + exit(code) */
_Noreturn void sc_die_msg(int code, const char *message, const char *hint);  /* one-shot */
```

### Usage

```c
/* Full builder */
ScError *error = sc_error_new("Config could not be loaded");
sc_error_add_cause(error, "file not found: ~/.config/app/config.toml");
sc_error_set_hint(error, "Run 'app init' to create a default config");
sc_error_set_code(error, 2);
sc_die(error);   /* renders to stderr, frees, exit(2) */

/* One-shot */
sc_die_msg(2, "No config file found", "Run 'app init' to create one");

/* Render without exiting (e.g. to show several errors) */
sc_error_print(error);          /* to sc_output_stream() - capture-able */
sc_error_print_stderr(error);   /* to stderr, output stream untouched */
sc_error_free(error);
```

### Behavior & safety

- **No signal handling:** this is purely a rendering/reporting convention; crash signals are never trapped (debuggers and sanitizers keep working).
- **Strings are copied and sanitized** at the builder entry points (the ANSI-injection trust boundary), so untrusted text in messages/causes/hints cannot inject escape sequences.
- `sc_error_print` writes to the (thread-local) output stream like every widget – use it in captures and tests. `sc_die`/`sc_error_print_stderr` always write to **stderr**, so errors stay visible when stdout is piped.
- Rendering reuses the alert/panel infrastructure (`SC_ALERT_ERROR` preset): bold message, dim `caused by:` lines, `Hint:` block.
- All entry points are `NULL`-safe.

### Bindings

| Language | API |
|----------|-----|
| C++ | `sparcli::ErrorReport("msg").cause(…).hint(…).code(2)` with `print()` / `print_stderr()` / `[[noreturn]] die()`; one-shot `sparcli::die(code, msg, hint)` |
| Rust | `sparcli::ErrorReport::new("msg").cause(…).hint(…).code(2)` with `print()` / `print_stderr()` / `die() -> !` |
| Python | `sc.ErrorReport("msg").cause(…).hint(…).code(2)` with `print()` / `print_stderr()` / `die()` (raises `SystemExit`, never calls C `exit()`) |

---

## Logging

Leveled, colored terminal output plus plain-text file sinks. Header: `log/sparcli_log.h`.

```
[14:32:07] INFO  Server started on port 8080      main.c:42
[14:32:08] WARN  Config not found, using defaults config.c:17
[14:32:09] ERROR Connection failed: timeout       net.c:103
```

Two flavors share one engine:

- **Global logger** (`sc_log_*`): process-wide, ready to use – a colored stderr sink at `SC_LOG_INFO` plus optional file sinks.
- **Handle-based loggers** (`sc_logger_*`): independent instances with their own sinks and options.

### Levels

```c
typedef enum { SC_LOG_DEBUG = 0, SC_LOG_INFO, SC_LOG_WARN, SC_LOG_ERROR, SC_LOG_OFF } ScLogLevel;
```

| Level | Tag color | Meaning |
|-------|-----------|---------|
| `SC_LOG_DEBUG` | dim magenta | Diagnostic detail |
| `SC_LOG_INFO` | cyan | Normal progress |
| `SC_LOG_WARN` | yellow | Unexpected but recoverable |
| `SC_LOG_ERROR` | red | Operation failed |
| `SC_LOG_OFF` | – | Sink/logger disabled |

### Global logger

```c
sc_log_set_level(SC_LOG_DEBUG);               /* stderr sink level; default INFO */
sc_log_add_file("app.log", SC_LOG_DEBUG);     /* plain-text file sink (appends) */
sc_log_set_opts((ScLoggerOpts){ .show_source = true });

sc_log_info("server started on port %d", 8080);
sc_log_warn("config not found, using defaults");
sc_log_error("connection failed: %s", strerror(errno));
sc_log_debug("raw packet: %zu bytes", n);

sc_log_reset();                               /* close files, restore defaults */
```

The `sc_log_debug/info/warn/error` macros pass `__FILE__`/`__LINE__` automatically (rendered as a dim suffix when `show_source` is set). The stderr sink emits colors only when stderr is a terminal.

### Handle-based loggers

```c
ScLogger *logger = sc_logger_new((ScLoggerOpts){ .hide_timestamps = true });
sc_logger_add_terminal(logger, stderr, SC_LOG_INFO);     /* colored; stream borrowed */
sc_logger_add_file(logger, "debug.log", SC_LOG_DEBUG);   /* plain text; owned */
sc_logger_info(logger, "subsystem ready (%d workers)", 4);
sc_logger_free(logger);                                   /* closes file sinks */
```

### ScLoggerOpts

| Field | Description |
|-------|-------------|
| `level` | Minimum level processed at all; zero-init = `SC_LOG_DEBUG` (everything) |
| `hide_timestamps` | `true` = no dim `[HH:MM:SS]` prefix |
| `show_source` | `true` = dim `file:line` suffix (from the log macros) |
| `markup` | `true` = parse `[tag]` markup in messages |
| `ansi` | ANSI passthrough for message text; zero-init inherits the global |

### Behavior & safety

- **Render once:** every record is rendered once (colored); terminal sinks receive the colored bytes, file sinks the ANSI-stripped plain text – both always contain the same characters.
- **Independent of the widget output:** sinks own their `FILE *` targets; logging never touches the thread-local `sc_output_stream()`, so it cannot disturb captures or an active pager.
- **Trust boundary:** the formatted message is sanitized (or markup-parsed) exactly once; injected escape sequences in message arguments cannot reach the terminal.
- **Format strings are developer-controlled:** never pass user input as the format argument (classic printf rule); the compiler checks formats via `__attribute__((format))`.
- **Thread safety:** configure the global logger first (`set_level`/`add_file`/`set_opts` are set-once), then `sc_log_*` calls are safe from any thread – each record is written with a single `fputs`, so lines never interleave. Handle-based loggers are not internally synchronized.

### Bindings

The bindings pass messages as **data** (internally `"%s"` + string), so `%` characters in messages are always literal – there is no format-string surface in C++/Rust/Python.

| Language | API |
|----------|-----|
| C++ | `sparcli::Logger` (RAII; `add_terminal`/`add_file`/`level` + `debug/info/warn/error`) and `sparcli::logging::set_level/add_file/info/...` for the global logger |
| Rust | `sparcli::Logger` (file sinks) + `sparcli::log::{set_level, add_file, info, warn, ...}` for the global logger |
| Python | `sc.Logger` (`add_terminal(sys.stderr)`/`add_file` + `debug/info/warning/error`) and `sc.log_set_level/log_add_file/log_info/...` for the global logger |

---

## Argument Parser

A declarative, builder-style argument parser ("clap for C"): describe the command tree once, and `sc_args_parse` handles `--help`/`--version`, validation, pretty error reporting with did-you-mean suggestions, typed value conversion and zsh completion generation. Header: `args/sparcli_args.h`.

### Tutorial: a complete tool in 40 lines

```c
#include <sparcli.h>
#include <string.h>

int main(int argc, char **argv) {
    /* 1. Describe the interface */
    ScArgs *args = sc_args_new((ScArgsOpts){
        .prog = "mytool", .version = "1.0.0",
        .about = "Builds and cleans projects",
    });
    ScArgsCmd *root = sc_args_root(args);
    sc_args_flag(root, "verbose", 'v', "Explain what is being done");

    ScArgsCmd *build = sc_args_subcommand(root, "build", "Build a target");
    sc_args_opt(build, "jobs", 'j', SC_ARG_INT, "N", "Parallel jobs");
    sc_args_opt_default(build, "jobs", "4");
    sc_args_opt(build, "mode", 'm', SC_ARG_STR, "MODE", "Build mode");
    sc_args_opt_choices(build, "mode", (const char *[]){ "debug", "release", NULL });
    sc_args_positional(build, "TARGET", SC_ARG_STR, "What to build", true, false);

    ScArgsCmd *clean = sc_args_subcommand(root, "clean", "Remove artifacts");
    sc_args_flag(clean, "force", 'f', "Skip confirmation");

    /* 2. Parse (handles --help/--version/errors internally) */
    ScArgsStatus status;
    const ScArgsCmd *matched = sc_args_parse(args, argc, argv, &status);
    if (status != SC_ARGS_MATCHED) {
        sc_args_free(args);
        return status == SC_ARGS_HANDLED ? 0 : 2;
    }

    /* 3. Dispatch on the matched command and read typed values */
    if (strcmp(sc_args_cmd_name(matched), "build") == 0) {
        long jobs = sc_args_get_int(args, "jobs");          /* 4 if absent */
        const char *target = sc_args_get_str(args, "TARGET");
        bool verbose = sc_args_get_flag(args, "verbose");   /* parent flag */
        /* ... build it ... */
    }
    sc_args_free(args);
    return 0;
}
```

What the user gets for free:

- `mytool --help` / `mytool build --help` – help screens rendered with sparcli widgets (bold header, cyan section headings, aligned tables, grouped commands)
- `mytool -V` / `mytool --version`
- `mytool buidl` → red error panel: *Unknown command 'buidl'. Hint: Did you mean 'build'?*
- `mytool build --jobs many` → *Invalid value 'many' for option '--jobs'. Hint: expected an integer*
- `mytool build --mode turbo` → *Hint: valid choices: debug, release*
- `sc_args_print_zsh_completion(args)` → a complete `#compdef` script

### Building the tree

| Function | Purpose |
|----------|---------|
| `sc_args_new(opts)` / `sc_args_free(a)` | Create/destroy the parser (strings copied) |
| `sc_args_root(a)` | The root command node |
| `sc_args_subcommand(parent, name, about)` | Add a subcommand (arbitrary nesting) |
| `sc_args_cmd_group(cmd, "Heading")` | Help section heading for a command |
| `sc_args_flag(cmd, "name", 'n', help)` | Boolean flag (`--name` / `-n`) |
| `sc_args_opt(cmd, "name", 'n', type, "METAVAR", help)` | Typed value option |
| `sc_args_opt_default(cmd, "name", "value")` | Default value (shown in help) |
| `sc_args_opt_choices(cmd, "name", choices)` | Restrict to fixed values (NULL-terminated array) |
| `sc_args_opt_required(cmd, "name")` | Parse fails when absent |
| `sc_args_positional(cmd, "NAME", type, help, required, variadic)` | Positional slot |

Value types: `SC_ARG_STR` (default), `SC_ARG_INT`, `SC_ARG_DOUBLE`, `SC_ARG_COLOR` (color name, `#RRGGBB` or `R,G,B`). "Enums" are `SC_ARG_STR` + `sc_args_opt_choices`.

### Parsing & reading results

```c
ScArgsStatus status;   /* SC_ARGS_MATCHED / SC_ARGS_HANDLED / SC_ARGS_ERROR */
const ScArgsCmd *matched = sc_args_parse(args, argc, argv, &status);

const char *s   = sc_args_get_str(args, "name");     /* value or default; NULL = none */
long n          = sc_args_get_int(args, "jobs");
double x        = sc_args_get_double(args, "scale");
bool flag       = sc_args_get_flag(args, "verbose");
int choice      = sc_args_get_enum(args, "mode");     /* index into choices; -1 = none */
ScColor color   = sc_args_get_color(args, "accent");
bool given      = sc_args_present(args, "jobs");      /* defaults do not count */

size_t count;
const char *const *extra = sc_args_get_many(args, "EXTRA", &count);  /* variadic positional */

const char *leaf = sc_args_selected_command(args);
```

Getters search the matched command **and its ancestors**, so global flags (`--verbose` on the root) are visible after a subcommand matched. Returned strings are borrowed and stay valid until `sc_args_free`.

### Supported syntax

| Form | Example |
|------|---------|
| Long option with separate value | `--jobs 8` |
| Long option with inline value | `--jobs=8` |
| Short option | `-j 8`, `-j8` |
| Combined short flags | `-vq` (multiple boolean flags) |
| Subcommands (nested) | `prog remote add` |
| Positionals + variadic tail | `prog build TARGET extra1 extra2` |
| End of options | `--` (everything after is positional, even `-x`) |
| Negative numbers as values | `--offset -5` |

### Behavior & safety

- **argv is untrusted input:** every token is ANSI-sanitized before it is stored or echoed back in error messages, so a malicious argument cannot inject escape sequences into the terminal.
- **Reserved options:** `--help`/`-h` on every command, `--version`/`-V` on the root (when a version was set). User options with the same short letters take precedence over the reserved short forms.
- **Errors are pretty errors** (the [Pretty Errors](#pretty-errors) module): red panel to stderr, message + hint, suggested exit code 2.
- **Did-you-mean:** unknown options/commands within Levenshtein distance ≤ 2 produce a suggestion hint.
- **Builder strings are copied**; getter results are borrowed from the parser.
- **Fuzzed:** `make fuzz` runs random argv vectors against a fixed tree under ASan/UBSan (`tests/fuzz/fuzz_args.c`).

### Help & completion output

```c
sc_args_print_help(args, NULL);            /* root help to the output stream */
sc_args_print_help(args, build_cmd);       /* per-command help */
sc_args_print_zsh_completion(args);        /* #compdef script */
```

A typical pattern is a hidden `completion` subcommand whose handler calls `sc_args_print_zsh_completion` – users install it with `mytool completion > ~/.zsh/completions/_mytool`.

### Bindings

C and C++ only (Rust has clap, Python has argparse/click - the parser would be unidiomatic there).

```cpp
// C++: sparcli::Args (RAII) + sparcli::ArgsCmd (borrowed builder nodes)
sparcli::Args args({ .prog = "mytool", .version = "1.0", .about = "Demo" });
auto root = args.root();
root.flag("verbose", 'v', "Verbose output");
auto build = root.subcommand("build", "Build the project");
build.opt("jobs", 'j', SC_ARG_INT, "N", "Parallel jobs").opt_default("jobs", "4")
     .opt_choices("mode", { "debug", "release" })
     .positional("TARGET", SC_ARG_STR, "Build target", true);

if (auto matched = args.parse(argc, argv)) {
    long jobs = args.get_int("jobs");
    auto target = args.get_str("TARGET");          // std::optional<std::string>
    auto extra = args.get_many("EXTRA");           // std::vector<std::string>
} else {
    return args.exit_code();                       // 0 for --help, 2 for errors
}
```
