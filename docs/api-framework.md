# sparcli – Framework API Reference (C)

The application-framework side of the C library: everything that supports the *application around the widgets* – where its files live, how it logs, how it reports errors, and how it parses its command line. The terminal/widget API (panels, tables, prompts, markup, …) is documented in [`api-c.md`](api-c.md).

All functions follow the library conventions: `sc_` prefix, zero-init-friendly opts structs, heap results paired with `free()`, and `SPARCLI_EXPORT` visibility. Headers live under `include/app/` and are included by the `sparcli.h` umbrella (sub-umbrella: `app/sparcli_app.h`).

## Table of contents

- [XDG Paths](#xdg-paths)
- [Pretty Errors](#pretty-errors)

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
