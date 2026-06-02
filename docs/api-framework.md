# sparcli – Framework API Reference (C)

The application-framework side of the C library: everything that supports the *application around the widgets* – where its files live, how it logs, how it reports errors, and how it parses its command line. The terminal/widget API (panels, tables, prompts, markup, …) is documented in [`api-c.md`](api-c.md).

All functions follow the library conventions: `sc_` prefix, zero-init-friendly opts structs, heap results paired with `free()`, and `SPARCLI_EXPORT` visibility. Headers live under `include/app/` and are included by the `sparcli.h` umbrella (sub-umbrella: `app/sparcli_app.h`).

## Table of contents

- [XDG Paths](#xdg-paths)
- [Pretty Errors](#pretty-errors)
- [Logging](#logging)

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
