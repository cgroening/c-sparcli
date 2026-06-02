#!/bin/sh
# Golden-file driver for the sparcli CLI output commands.
#
# Runs every non-interactive subcommand with fixed arguments and fixtures;
# the combined stdout+stderr byte stream is diffed against expected.txt by
# `make test-cli-check` (regenerate with `make test-cli-golden`). Output is
# deterministic because stdout is a pipe (terminal width falls back to 80)
# and layout-sensitive cases pass an explicit --width.
#
# Usage: run_output.sh ./sparcli
set -u

BIN=$1
FIXTURES=$(dirname "$0")/fixtures

section() {
    printf '\n===== %s =====\n' "$1"
}

# Runs a command that is expected to fail, echoing its exit code, so error
# paths are part of the golden snapshot without aborting the script.
expect_fail() {
    "$@" 2>&1
    printf 'exit: %s\n' "$?"
}

# ── print ────────────────────────────────────────────────────────────────────

section "print: markup"
"$BIN" print "[bold green]Hello[/] [red]World[/]"

section "print: multiple args joined"
"$BIN" print "[bold]first[/]" "second" "third"

section "print: --no-markup leaves tags verbatim"
"$BIN" print --no-markup "[bold]not bold[/]"

section "print: --no-color strips colors but keeps markup styling tags out"
"$BIN" print --no-color "[bold green]plain[/] text"

section "print: stdin input"
printf 'from [cyan]stdin[/]\n' | "$BIN" print

section "print: centered"
"$BIN" print --align center --width 40 "centered"

section "print: right-aligned"
"$BIN" print --align right --width 40 "right edge"

# ── panel ────────────────────────────────────────────────────────────────────

section "panel: basic"
"$BIN" panel --width 40 "Panel content"

section "panel: title + subtitle"
"$BIN" panel --width 40 --title "Title" --subtitle "v1.0" "Body"

section "panel: markup title and content"
"$BIN" panel --width 40 --title "[bold cyan]Status[/]" "[green]All good[/]"

section "panel: border styles"
for border in none ascii single double rounded thick; do
    "$BIN" panel --width 30 --border "$border" "border: $border"
done

section "panel: colored border and background"
"$BIN" panel --width 40 --color magenta --bg "#222233" "Colored panel"

section "panel: padding and margin"
"$BIN" panel --width 40 --padding 1,2,1,2 --margin 1,0,1,4 "Padded"

section "panel: content alignment"
"$BIN" panel --width 40 --align center "Centered content"
"$BIN" panel --width 40 --align right "Right content"

section "panel: title alignment"
"$BIN" panel --width 40 --title "Left" --title-align left "x"
"$BIN" panel --width 40 --title "Center" --title-align center "x"
"$BIN" panel --width 40 --title "Right" --title-align right "x"

section "panel: stdin content with multiple lines"
printf 'line one\nline two\nline three\n' | "$BIN" panel --width 40

section "panel: no-color"
"$BIN" panel --no-color --width 40 --color red --title "[bold]T[/]" "body"

# ── rule ─────────────────────────────────────────────────────────────────────

section "rule: plain"
"$BIN" rule --width 40

section "rule: title centered (default)"
"$BIN" rule --width 40 "Section"

section "rule: title left / right"
"$BIN" rule --width 40 --align left "Left"
"$BIN" rule --width 40 --align right "Right"

section "rule: markup title, color, double border"
"$BIN" rule --width 40 --border double --color cyan "[bold]Bold[/]"

section "rule: margins"
"$BIN" rule --width 30 --margin 1,0,1,5 "indented"

# ── table ────────────────────────────────────────────────────────────────────

section "table: CSV fixture with header row"
"$BIN" table --header-row --width 70 "$FIXTURES/sample.csv"

section "table: quoted fields, embedded delimiter/quotes/newlines"
"$BIN" table --header-row --no-markup "$FIXTURES/sample.csv"

section "table: stdin, no header"
printf 'a,b,c\n1,2,3\n' | "$BIN" table

section "table: TSV input"
printf 'Key\tValue\nport\t8080\nhost\tlocalhost\n' | "$BIN" table --tsv --header-row

section "table: custom delimiter"
printf 'a;b\n1;2\n' | "$BIN" table --delim ';'

section "table: border styles and colors"
printf 'A,B\n1,2\n' | "$BIN" table --header-row --border double --color cyan
printf 'A,B\n1,2\n' | "$BIN" table --header-row --border ascii

section "table: striped, no inner horizontal lines"
printf 'N,V\na,1\nb,2\nc,3\nd,4\n' | "$BIN" table --header-row --striped \
    --stripe-bg "#333333" --no-inner-h

section "table: title, alignment, max rows"
printf 'N,V\na,1\nb,2\nc,3\nd,4\n' | "$BIN" table --header-row \
    --title "Limited" --align right --max-rows 2

section "table: markup in cells"
printf 'Status,Job\n[green]ok[/],build\n[red]fail[/],test\n' | \
    "$BIN" table --header-row

section "table: header column"
printf 'Q1,100\nQ2,150\nQ3,90\n' | "$BIN" table --header-col

# ── list ─────────────────────────────────────────────────────────────────────

section "list: bullet (default)"
"$BIN" list "first" "second" "third"

section "list: markers"
for marker in number alpha alpha-upper roman roman-upper; do
    "$BIN" list --marker "$marker" "one" "two" "three"
done

section "list: custom bullet, marker color, indent, gap"
"$BIN" list --bullet "→" --marker-color cyan --indent 4 --gap 1 "a" "b"

section "list: stdin items with markup"
printf '[bold]bold item[/]\nplain item\n' | "$BIN" list

section "list: word wrap within width"
"$BIN" list --width 30 --marker number \
    "a rather long list item that needs to wrap onto multiple lines"

# ── tree ─────────────────────────────────────────────────────────────────────

section "tree: fixture"
"$BIN" tree "$FIXTURES/tree.txt"

section "tree: stdin, ascii border, no guides"
printf 'root\n  a\n  b\n    b1\n' | "$BIN" tree --border ascii
printf 'root\n  a\n  b\n    b1\n' | "$BIN" tree --no-guide

section "tree: connector color, tabs as indentation"
printf 'top\n\tmid\n\t\tdeep\n' | "$BIN" tree --color green

section "tree: error on skipped indentation level"
expect_fail sh -c "printf 'root\n        too deep\n' | $BIN tree"

# ── kv ───────────────────────────────────────────────────────────────────────

section "kv: fixture"
"$BIN" kv "$FIXTURES/kv.txt"

section "kv: colors, fixed key width, wrap"
"$BIN" kv --key-color cyan --val-color yellow --key-width 14 --wrap \
    --width 50 "$FIXTURES/kv.txt"

section "kv: tab delimiter from stdin"
printf 'host\tlocalhost\nport\t8080\n' | "$BIN" kv

section "kv: custom separator and gap"
printf 'a: 1\nb: 2\n' | "$BIN" kv --sep " = " --gap 1

# ── alert ────────────────────────────────────────────────────────────────────

section "alert: all levels"
for level in info debug warning error success; do
    "$BIN" alert "$level" "This is a $level alert"
done

section "alert: stdin body with markup"
printf 'Deploy [bold]v2.1[/] finished\n' | "$BIN" alert success

# ── badge ────────────────────────────────────────────────────────────────────

section "badge: plain, colored, custom caps"
"$BIN" badge "v1.2"
"$BIN" badge --color green --bold "passing"
"$BIN" badge --color white --bg red --pad 1 "FAIL"
"$BIN" badge --left "<" --right ">" "angle"

# ── columns ──────────────────────────────────────────────────────────────────

section "columns: basic"
"$BIN" columns "left column" "middle column" "right column"

section "columns: separator, gap, stdin"
printf 'alpha\nbeta\ngamma\n' | "$BIN" columns --sep single --gap 2

# ── ANSI-injection protection ────────────────────────────────────────────────

section "sanitize: injected escape codes and control bytes are stripped"
"$BIN" print "$(printf 'safe\033[31minjected\033]0;evil-title\007text')"
"$BIN" panel --border single "$(printf 'evil\033[31mred\007bell')"
printf 'Name,Status\njob\033[31m1,ok\033]0;x\007done\n' \
    | "$BIN" table --header-row

section "sanitize: --allow-ansi passes well-formed escape codes through"
"$BIN" print --allow-ansi "$(printf 'ok\033[31mred\033[0m')"
"$BIN" panel --border single --allow-ansi \
    "$(printf '\033[32mgreen\033[0m text')"

# ── global flags / errors ────────────────────────────────────────────────────

section "help: command list"
"$BIN" --help

section "help: subcommand"
"$BIN" table --help

section "errors"
expect_fail "$BIN" bogus-command
expect_fail "$BIN" --bogus-flag
expect_fail "$BIN" panel --border bogus "x"
expect_fail "$BIN" rule --color notacolor "x"
expect_fail "$BIN" print --align diagonal "x"
expect_fail "$BIN" table "$FIXTURES/does-not-exist.csv"
expect_fail "$BIN" alert bogus-level "x"
expect_fail "$BIN" badge
expect_fail "$BIN" alert
