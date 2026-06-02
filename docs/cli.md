# sparcli – Command-line tool reference

The `sparcli` binary exposes the library's output and input widgets as shell subcommands, so everything sparcli renders in C is also available in zsh/bash scripts – inspired by [rich-cli](https://github.com/Textualize/rich-cli) (output) and [gum](https://github.com/charmbracelet/gum) (prompts).

```sh
sparcli print "[bold green]Hello[/] from the shell"
name=$(sparcli input "Your name:")
sparcli confirm "Continue?" && echo "onwards"
```

- [Installation](#installation)
- [Global conventions](#global-conventions)
- [Output commands](#output-commands): [print](#print) · [panel](#panel) · [rule](#rule) · [table](#table) · [list](#list) · [tree](#tree) · [kv](#kv) · [alert](#alert) · [badge](#badge) · [columns](#columns)
- [Streaming commands](#streaming-commands): [spin](#spin) · [progress](#progress)
- [Input commands](#input-commands): [confirm](#confirm) · [input](#input) · [password](#password) · [number](#number) · [textarea](#textarea) · [select](#select) · [fuzzy](#fuzzy) · [date](#date)
- [Data formats](#data-formats)
- [zsh scripting patterns](#zsh-scripting-patterns)

---

## Installation

The CLI is built by the default `make` target and installed by `make install`:

```sh
make                # builds the library and the ./sparcli binary
sudo make install   # installs sparcli to $(PREFIX)/bin (default /usr/local/bin)
                    # and the zsh completion to $(PREFIX)/share/zsh/site-functions
```

The binary is statically linked against `libsparcli.a`, so it has no runtime dependency on the installed library. The zsh completion (`completions/_sparcli`) is picked up automatically once it is in `$fpath` (re-run `compinit` or restart the shell).

---

## Global conventions

### Synopsis

```
sparcli [global options] <command> [options] [arguments]
```

`sparcli --help` lists all commands; `sparcli <command> --help` shows the options of one command.

### Global options

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show the command list and exit |
| `-V`, `--version` | Show the version and exit |
| `--no-color` | Strip all ANSI colors/attributes from the output |
| `--no-markup` | Treat `[tag]` markup as literal text |
| `--allow-ansi` | Pass raw ANSI escape codes in input text through (default: stripped) |

`--no-color`, `--no-markup` and `--allow-ansi` are also accepted after the command name (e.g. `sparcli panel --no-color "x"`).

### Markup

All text content – `print` text, panel/rule/table titles, table cells, list items, alert bodies, input prompts – is parsed as [Rich-compatible inline markup](api-c.md#markup) by default:

```sh
sparcli print "[bold red]Error:[/] [dim]something went wrong[/]"
sparcli list "[green]✔[/] done" "[yellow]…[/] running"
```

`--no-markup` disables parsing for one invocation; the text is then rendered exactly as given. Exception: `kv` values and `badge` text are always literal (the underlying widgets take plain strings).

### Colors

Wherever a command takes a `COLOR` value, three forms are accepted:

| Form | Example |
|------|---------|
| Named ANSI color | `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, `black` |
| Hex RGB | `#ff8800` |
| Decimal RGB | `255,136,0` |

`--no-color` (or a non-empty [`NO_COLOR`](https://no-color.org) environment variable) strips every ANSI escape sequence from the output – including colors that come from markup tags.

### ANSI-injection protection

All input text (arguments, stdin, CSV cells) is **sanitized by default**: raw ANSI escape sequences and control bytes are removed before rendering, so untrusted data (log lines, file contents, API responses) cannot inject terminal escape codes or shift widget frames:

```sh
printf 'job\033[31m1,ok\033]0;evil\007done\n' | sparcli table   # renders clean
```

`--allow-ansi` opts out for one invocation: well-formed escape sequences pass through (stray control bytes are still removed) and all width math accounts for them, so panels and tables stay aligned:

```sh
ls --color=always | sparcli panel --allow-ansi --title "Files"
```

### Reading input

Output commands take their content from (in order of precedence):

1. positional arguments (joined with spaces, or one item per argument for list-like commands),
2. a `FILE` argument (`table`, `tree`, `kv`) – `-` means stdin,
3. stdin, when neither is given.

File and stdin input is capped at **64 MiB**; larger input is rejected with an error so untrusted data cannot exhaust memory.

### Edge values (padding/margin)

`EDGES` values accept three forms: `N` (all four sides), `V,H` (top/bottom, left/right) or `T,R,B,L` (CSS order).

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | Success; for `confirm`: answered yes |
| `1` | Input cancelled (Esc / Ctrl-C); for `confirm`: answered no |
| `2` | Error: bad usage, invalid data, unreadable file, or no TTY for an input command |

`spin` is the exception: it propagates the wrapped command's exit code (`128+N` for signals).

### Terminal width

Layout commands use the terminal width when `--width` is not given. When stdout is not a terminal (pipe, file), the width falls back to 80 columns, which keeps output deterministic in scripts and tests.

---

## Output commands

Render styled output to stdout. Exit code: `0`, or `2` on bad usage/data.

### print

```
sparcli print [options] [TEXT...]
```

Prints markup-styled text. Multiple `TEXT` arguments are joined with spaces; without arguments the text is read from stdin.

| Option | Description |
|--------|-------------|
| `--align left\|center\|right` | Align the text block within `--width` |
| `--width N` | Alignment width (0 = terminal width) |

```sh
sparcli print "[bold]Headline[/]"
sparcli print --align center "[dim]centered[/]"
git log -1 --format=%s | sparcli print
```

### panel

```
sparcli panel [options] [TEXT...]
```

Draws a bordered panel around `TEXT` (or stdin).

| Option | Description |
|--------|-------------|
| `--title TEXT` | Title on the top edge |
| `--subtitle TEXT` | Second title on the bottom edge (right-aligned) |
| `--title-align left\|center\|right` | Title alignment |
| `--border STYLE` | `none` `ascii` `single` `double` `rounded` (default) `thick` |
| `--color COLOR` | Border color |
| `--bg COLOR` | Content background color |
| `--align left\|center\|right` | Content alignment |
| `--padding EDGES` | Inner padding (default `0,1,0,1`) |
| `--margin EDGES` | Outer margin |
| `--width N` | Panel width (0 = fit content) |
| `--full-width` | Stretch to the terminal width |

```sh
echo "All systems operational" | sparcli panel --title "Status" --color green
sparcli panel --border double --padding 1 --width 60 "Centered look" --align center
```

### rule

```
sparcli rule [options] [TITLE]
```

Draws a horizontal rule across the terminal (or `--width` columns), optionally labelled.

| Option | Description |
|--------|-------------|
| `--border STYLE` | Line style (default `single`) |
| `--color COLOR` | Line color |
| `--align left\|center\|right` | Title position (default `center`) |
| `--width N` | Fixed rule width (0 = terminal width) |
| `--margin EDGES` | Outer margin (top/bottom = blank lines, left/right = indent) |

```sh
sparcli rule
sparcli rule "Results" --color cyan
sparcli rule "[bold]Phase 2[/]" --border double --align left
```

### table

```
sparcli table [options] [FILE]
```

Renders CSV data from `FILE` (or stdin) as a table. Quoted fields per RFC 4180 are supported: embedded delimiters, embedded newlines (multi-line cells) and `""` escapes. Cell text is parsed as markup unless `--no-markup` is given.

| Option | Description |
|--------|-------------|
| `--tsv` | Tab-separated input |
| `--delim CHAR` | Custom field delimiter (default `,`) |
| `--header-row` | First row is the header (rendered bold) |
| `--header-col` | First column is a header column |
| `--border STYLE` | Border style (default `single`) |
| `--color COLOR` | Outer border color |
| `--inner-color COLOR` | Inner separator color |
| `--no-outer` | Hide the outer frame |
| `--no-inner-h` | Hide row separators |
| `--no-inner-v` | Hide column separators |
| `--striped` | Alternate row backgrounds |
| `--stripe-bg COLOR` | Stripe background color |
| `--title TEXT` | Table title |
| `--align left\|center\|right` | Default column alignment |
| `--width N` | Total table width (0 = fit content) |
| `--max-rows N` | Show at most N rows (with truncation indicator) |

```sh
sparcli table data.csv --header-row
df -h | tr -s ' ' '\t' | sparcli table --tsv --header-row
printf 'Status,Job\n[green]ok[/],build\n[red]fail[/],test\n' | sparcli table --header-row
```

### list

```
sparcli list [options] [ITEM...]
```

Renders a bulleted or numbered list. Each `ITEM` argument is one entry; without arguments each stdin line becomes an entry.

| Option | Description |
|--------|-------------|
| `--marker STYLE` | `bullet` (default) `number` `alpha` `alpha-upper` `roman` `roman-upper` |
| `--bullet STR` | Bullet character (marker `bullet` only) |
| `--marker-color COLOR` | Marker color |
| `--indent N` | Left indent in columns |
| `--gap N` | Blank lines between items |
| `--width N` | List width; long items word-wrap with hanging indent |

```sh
sparcli list "first" "second" "third"
ls *.c | sparcli list --marker number
```

### tree

```
sparcli tree [options] [FILE]
```

Renders indented lines from `FILE` (or stdin) as a tree. See [Data formats](#tree-indentation-format).

| Option | Description |
|--------|-------------|
| `--border STYLE` | Connector style: `ascii` `single` (default) `double` `rounded` `thick` |
| `--color COLOR` | Connector color |
| `--indent N` | Spaces per indentation level (default 2); tabs count as one level each |
| `--no-guide` | Hide the vertical guide lines |

```sh
sparcli tree structure.txt
printf 'app\n  api\n    auth\n  web\n' | sparcli tree --color blue
```

### kv

```
sparcli kv [options] [FILE]
```

Renders `key: value` (or `key<TAB>value`) lines from `FILE` (or stdin) as an aligned key/value list. Values are literal text (the kv widget does not support inline markup); use `--key-color` / `--val-color` for styling.

| Option | Description |
|--------|-------------|
| `--delim CHAR` | Input delimiter (default: tab, then `:`) |
| `--sep STR` | Output separator between key and value (default: two spaces) |
| `--key-width N` | Fixed key column width (0 = widest key) |
| `--key-color COLOR` | Key color |
| `--val-color COLOR` | Value color |
| `--wrap` | Word-wrap long values with hanging indent |
| `--gap N` | Blank lines between items |
| `--width N` | Total width (0 = terminal width) |

```sh
sparcli kv info.txt --key-color cyan
printf 'host\tlocalhost\nport\t8080\n' | sparcli kv
```

### alert

```
sparcli alert [options] LEVEL [TEXT...]
```

Shows a full-width alert panel with a level icon and color. `LEVEL` is one of `info`, `debug`, `warning`, `error`, `success`. Without `TEXT` the body is read from stdin.

```sh
sparcli alert error "Build failed"
make 2>&1 | tail -1 | sparcli alert warning
```

### badge

```
sparcli badge [options] TEXT...
```

Prints an inline styled badge like `[ok]`. The badge text is literal (no markup).

| Option | Description |
|--------|-------------|
| `--color COLOR` | Text color |
| `--bg COLOR` | Background color |
| `--bold` | Bold text |
| `--left STR` / `--right STR` | Cap strings (default `[` / `]`) |
| `--pad N` | Spaces inside the caps |

```sh
sparcli badge --color green --bold "passing"
sparcli badge --color white --bg red --pad 1 "FAIL"
```

### columns

```
sparcli columns [options] [ITEM...]
```

Lays out items side by side, one column per `ITEM` argument (or per stdin line). Items may contain newlines for multi-line columns.

| Option | Description |
|--------|-------------|
| `--gap N` | Spaces between columns |
| `--sep STYLE` | Vertical separator style (default: none) |
| `--sep-color COLOR` | Separator color |
| `--width N` | Total width (0 = fit content) |

```sh
sparcli columns "$(cat left.txt)" "$(cat right.txt)" --sep single
```

---

## Streaming commands

### spin

```
sparcli spin [options] -- COMMAND [ARGS...]
... | sparcli spin [options]
```

Runs `COMMAND` with an animated spinner, then a ✔/✖ finish mark. The spinner is drawn on the terminal (`/dev/tty`), so the command's stdout/stderr pass through untouched – pipes and redirection keep working. The command's exit code is propagated (`128+N` if it was killed by a signal); without a terminal the command simply runs without animation.

Without `-- COMMAND`, the spinner runs while stdin is consumed to EOF (for the end of a pipeline).

| Option | Description |
|--------|-------------|
| `--title TEXT` | Label next to the spinner |
| `--spinner STYLE` | `braille` (default) `pipe` `dots` `arrow` |
| `--color COLOR` | Spinner color |

```sh
sparcli spin --title "Compiling" -- make -j8
sparcli spin --title "Downloading" -- curl -sLO https://example.com/file.tar.gz
```

### progress

```
... | sparcli progress [options]
```

Draws a progress bar driven by numeric values read from stdin (one per line). With `--total N` each value means "N of total done"; without it, values are ratios between 0 and 1. Non-numeric lines are ignored, so interleaved log output is fine. The bar finishes when stdin closes. Intended for interactive use (the bar redraws in place with `\r`).

| Option | Description |
|--------|-------------|
| `--total N` | Value that counts as 100% |
| `--type STYLE` | `block` (default) `ascii` `line` `shaded` |
| `--label TEXT` | Label left of the bar |
| `--color COLOR` | Fill color |
| `--width N` | Total line width (0 = terminal width) |
| `--no-percent` | Hide the percentage |

```sh
for i in $(seq 100); do
    process_item "$i"
    echo "$i"
done | sparcli progress --total 100 --label "Processing"
```

---

## Input commands

All prompts run with bracketed-paste protection: pasted text is inserted literally (escape codes and control characters removed) – a pasted line break does not submit the field, and pasted characters cannot answer `confirm` or trigger shortcuts.

Interactive prompts. The contract that makes them script-friendly:

- the **widget UI is drawn on `/dev/tty`**, never on stdout;
- only the **raw result value** is written to stdout (one value per line);
- the **exit code** reports the outcome: `0` ok, `1` cancelled (Esc/Ctrl-C), `2` error or no TTY.

So command substitution and pipes compose naturally:

```sh
name=$(sparcli input "Name:")            # UI on the terminal, value in $name
branch=$(git branch | sparcli select)    # items in via pipe, choice out via stdout
sparcli confirm "Delete?" && rm -rf ./build
```

Options shared by **all** input commands:

| Option | Description |
|--------|-------------|
| `--accent COLOR` | Accent color of the widget (cursor, highlights) |
| `--hint TEXT` | Custom key-hint footer text |
| `--no-hint` | Hide the key-hint footer |
| `--no-markup` | Treat markup in the prompt as literal text |

### confirm

```
sparcli confirm [options] QUESTION...
```

Asks a yes/no question. The exit code carries the answer: `0` = yes, `1` = no or cancelled, `2` = error. Prints nothing unless `--print` is given.

| Option | Description |
|--------|-------------|
| `--default-yes` / `--default-no` | Preselected answer (default: yes) |
| `--yes LABEL` / `--no LABEL` | Custom choice labels |
| `--print` | Print `yes` or `no` to stdout |

```sh
sparcli confirm "Deploy to production?" && ./deploy.sh
sparcli confirm --default-no "Overwrite existing file?" || exit 1
```

### input

```
sparcli input [options] PROMPT...
```

Prompts for a line of text and prints it to stdout.

| Option | Description |
|--------|-------------|
| `--initial TEXT` | Pre-filled value |
| `--placeholder TEXT` | Dim placeholder shown while empty |
| `--max N` | Maximum number of characters (also caps typing) |
| `--filter FILTER` | Restrict input: `digits` `decimal` `alpha` `alnum` `no-space` |
| `--suggest TEXT` | Autocomplete suggestion, Tab accepts (repeatable) |
| `--boxed` | Draw the field inside a panel |
| `--border STYLE` | Box border style (with `--boxed`) |
| `--width N` | Field width (0 = terminal width) |

```sh
name=$(sparcli input "Your name:" --placeholder "Ada Lovelace")
port=$(sparcli input "Port:" --filter digits --max 5 --initial 8080)
city=$(sparcli input "City:" --suggest Berlin --suggest Hamburg --suggest Munich)
```

### password

```
sparcli password [options] PROMPT...
```

Prompts for a masked secret and prints it to stdout.

| Option | Description |
|--------|-------------|
| `--mask STR` | Mask character (default `*`; empty string hides the length) |
| `--max N` | Maximum number of characters |
| `--boxed` / `--border STYLE` / `--width N` | Boxed rendering |

```sh
token=$(sparcli password "API token:" --mask "•")
```

### number

```
sparcli number [options] PROMPT...
```

Prompts for a number; ↑/↓ step the value. Prints the **exact text** of the entered value (always `.`-decimal, clamp-consistent), so scripts and decimal-safe tools get a precise string instead of a float round-trip.

| Option | Description |
|--------|-------------|
| `--initial N` | Initial value |
| `--start-empty` | Start with an empty field (Enter on empty is ignored) |
| `--min N` / `--max N` | Clamp range (applied when max > min) |
| `--step N` | ↑/↓ step size |
| `--decimals N` | Decimal places shown |
| `--decimal-sep . \| ,` | Separator shown/accepted while editing |
| `--boxed` / `--border STYLE` / `--width N` | Boxed rendering |

```sh
amount=$(sparcli number "Amount (EUR):" --decimals 2 --decimal-sep , --min 0 --start-empty)
count=$(sparcli number "How many?" --initial 1 --min 1 --max 99 --step 1)
```

### textarea

```
sparcli textarea [options] PROMPT...
```

Prompts for multi-line text (Enter inserts a newline, **Ctrl-D submits**) and prints it to stdout.

| Option | Description |
|--------|-------------|
| `--initial TEXT` | Pre-filled value |
| `--placeholder TEXT` | Dim placeholder shown while empty |
| `--external-editor` | Ctrl-G opens `$VISUAL` / `$EDITOR` |
| `--editor CMD` | Specific editor command (implies `--external-editor`) |
| `--boxed` / `--border STYLE` / `--width N` | Boxed rendering |

```sh
notes=$(sparcli textarea "Release notes:" --external-editor)
```

### select

```
sparcli select [options] [ITEM...]
```

Choose one item – or several with `--multi` (Space toggles) – from a list. Items come from the `ITEM` arguments or, without arguments, from stdin (one per line). The chosen item(s) are printed to stdout, one per line.

| Option | Description |
|--------|-------------|
| `--prompt TEXT` | Heading shown above the list |
| `--multi` | Multi-select with checkboxes |
| `--max-visible N` | Viewport height (default 10, scrolls beyond) |

```sh
flavor=$(sparcli select vanilla chocolate pistachio)
branch=$(git branch --format='%(refname:short)' | sparcli select --prompt "Branch:")
toppings=(${(f)"$(sparcli select --multi cheese salami mushrooms)"})   # zsh array
```

### fuzzy

```
sparcli fuzzy [options] [ITEM...]
```

Incremental fuzzy search over a list; typing narrows the matches, Enter picks the highlighted one. Items from arguments or stdin. The chosen item is printed to stdout.

With `--tsv` (or `--delim`) each line is split into columns and shown as a table; the query searches all columns and the **first field** of the chosen row is printed.

| Option | Description |
|--------|-------------|
| `--prompt TEXT` | Search field label |
| `--max-visible N` | Viewport height (default 10) |
| `--tsv` | Tab-separated table view |
| `--delim CHAR` | Custom delimiter for the table view |
| `--header-row` | First input line provides the table headers |

```sh
file=$(find . -name '*.c' | sparcli fuzzy --prompt "Open:")
row=$(sqlite3 -separator $'\t' app.db 'SELECT id,name,email FROM users' | \
    sparcli fuzzy --tsv --prompt "User:")
```

### date

```
sparcli date [options]
```

Pick a date from a month-grid calendar (arrows move, PageUp/Down change month, Shift+PageUp/Down change year). The picked date is printed to stdout.

| Option | Description |
|--------|-------------|
| `--prompt TEXT` | Heading shown above the calendar |
| `--format FMT` | strftime output format (default `%Y-%m-%d`) |
| `--initial YYYY-MM-DD` | Initially selected date (default: today) |
| `--week-start monday\|sunday` | First day of the week |

```sh
deadline=$(sparcli date --prompt "Deadline?" --format "%d.%m.%Y")
```

---

## Data formats

### CSV (table, fuzzy --tsv)

- Fields are split on the delimiter (`,` by default, `\t` with `--tsv`, anything with `--delim`).
- A field starting with `"` is quoted: it may contain delimiters, newlines (multi-line cells) and `""` for a literal quote, until the closing `"`.
- Rows may have different field counts; short rows are padded with empty cells.
- CRLF line endings are handled.

### Tree indentation format

Each line is one node. Leading whitespace sets the depth: every `--indent` spaces (default 2) – or every tab – nest the node one level deeper under the previous shallower line.

```
project            depth 0
  src              depth 1 (child of project)
    main.c         depth 2 (child of src)
  docs             depth 1 (child of project)
README.md          depth 0 (new root)
```

A line that skips a level (e.g. depth 2 directly after depth 0) is an error (exit 2).

### kv line format

One pair per line: `key<TAB>value` or `key: value` (the first tab wins over the first colon; override with `--delim`). Whitespace after the delimiter is trimmed. Blank lines are skipped.

---

## zsh scripting patterns

**Command substitution + cancellation handling.** Every input command writes only the value to stdout and reports cancellation via the exit code:

```zsh
if name=$(sparcli input "Name:"); then
    print "hello, $name"
else
    print "cancelled (exit $?)"
fi
```

**Defaults on cancel:**

```zsh
env=$(sparcli select dev staging prod) || env="dev"
```

**Multi-select into an array** (`${(f)...}` splits on newlines):

```zsh
files=(${(f)"$(ls | sparcli select --multi)"})
print "selected ${#files} file(s)"
```

**Guard destructive actions:**

```zsh
sparcli confirm --default-no "Drop the database?" && dropdb myapp
```

**ZLE widget – fuzzy directory jump on Ctrl-J:**

```zsh
fuzzy-cd() {
    local dir
    dir=$(fd --type d | sparcli fuzzy --prompt "cd ") && cd "$dir"
    zle reset-prompt
}
zle -N fuzzy-cd
bindkey '^J' fuzzy-cd
```

**Pipelines with progress:**

```zsh
total=$(ls *.png | wc -l)
i=0
for img in *.png; do
    convert "$img" "${img%.png}.webp"
    print $((++i))
done | sparcli progress --total $total --label "Converting"
```

**Long-running commands with a spinner:**

```zsh
sparcli spin --title "Running tests" -- make test
```

Two complete, runnable demos ship with the repository: [`examples/cli_output_demo.zsh`](../examples/cli_output_demo.zsh) and [`examples/cli_input_demo.zsh`](../examples/cli_input_demo.zsh).
