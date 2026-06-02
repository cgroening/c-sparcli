# sparcli – Framework API Reference (C)

The application-framework side of the C library: everything that supports the *application around the widgets* – where its files live, how it logs, how it reports errors, and how it parses its command line. The terminal/widget API (panels, tables, prompts, markup, …) is documented in [`api-c.md`](api-c.md).

All functions follow the library conventions: `sc_` prefix, zero-init-friendly opts structs, heap results paired with `free()`, and `SPARCLI_EXPORT` visibility. Headers live under `include/app/` and are included by the `sparcli.h` umbrella (sub-umbrella: `app/sparcli_app.h`).

## Table of contents

- [XDG Paths](#xdg-paths)

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
