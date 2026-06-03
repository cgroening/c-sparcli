#!/usr/bin/env zsh
# sparcli CLI demo: output commands.
#
# Walks through every output subcommand of the `sparcli` binary - the
# shell-facing counterpart of the C library's output widgets. Run it from
# the repository root after `make` (or set SPARCLI to an installed binary):
#
#   ./examples/cli/output_demo.zsh
#   SPARCLI=/usr/local/bin/sparcli ./examples/cli/output_demo.zsh
#
# Full command reference: docs/cli.md

emulate -L zsh
set -e

bin=${SPARCLI:-./sparcli}
if [[ ! -x $bin ]]; then
    print -u2 "sparcli binary not found at '$bin' - run 'make' first"
    exit 1
fi

# ── print: markup text ──────────────────────────────────────────────────────
$bin rule "print" --color cyan
$bin print "[bold green]sparcli[/] renders [italic]rich markup[/] from the shell"
$bin print "Colors: [red]red[/] [yellow]yellow[/] [blue]blue[/] [magenta]magenta[/]"
$bin print --align center "[dim]centered text[/]"
$bin print --no-markup "[bold]markup can be disabled[/]"

# ── panel: bordered boxes ────────────────────────────────────────────────────
$bin rule "panel" --color cyan
$bin panel --title "Status" --border rounded --color green \
    "All systems [bold green]operational[/]"
print -l "Panels read stdin too." "Multi-line content works." | \
    $bin panel --title "[bold]From a pipe[/]" --subtitle "v1.0" --width 50

# ── rule: section separators ────────────────────────────────────────────────
$bin rule "rule" --color cyan
$bin rule
$bin rule "[bold]Centered title[/]"
$bin rule "left" --align left --border double --color magenta

# ── table: CSV/TSV rendering ─────────────────────────────────────────────────
$bin rule "table" --color cyan
printf 'Tool,Language,Stars\nrich,Python,50k\ngum,Go,18k\nsparcli,C,?\n' | \
    $bin table --header-row --color cyan
print "df -h as a table:"
df -h | tr -s ' ' '\t' | $bin table --tsv --header-row --max-rows 5

# ── list: bullets and numbering ──────────────────────────────────────────────
$bin rule "list" --color cyan
$bin list "plain bullet item" "[green]markup[/] in items" "third item"
print -l "first" "second" "third" | $bin list --marker roman --marker-color yellow

# ── tree: hierarchies from indented lines ───────────────────────────────────
$bin rule "tree" --color cyan
$bin tree --color blue << 'EOF'
project
  src
    main.c
    util.c
  docs
    api.md
README.md
EOF

# ── kv: aligned key/value lists ──────────────────────────────────────────────
$bin rule "kv" --color cyan
$bin kv --key-color cyan << 'EOF'
Name: sparcli
Purpose: styled terminal output
License: MIT
EOF

# ── alert: status panels ─────────────────────────────────────────────────────
$bin rule "alert" --color cyan
$bin alert info "Informational message"
$bin alert warning "Disk usage above [bold]80%[/]"
$bin alert success "Deployment finished"

# ── badge / columns ──────────────────────────────────────────────────────────
$bin rule "badge & columns" --color cyan
print -n "Build: "; $bin badge --color green --bold "passing"
print -n "Tests: "; $bin badge --color white --bg red --pad 1 "3 failed"
$bin columns --sep single --gap 2 \
    $'[bold]Column A[/]\nwith content' $'[bold]Column B[/]\nside by side'

# ── spin: spinner around a command ───────────────────────────────────────────
$bin rule "spin" --color cyan
$bin spin --title "Pretending to work" --spinner dots --color cyan -- sleep 2
spin_code=0
$bin spin -- false || spin_code=$?
print "spin propagates exit codes: spin -- false -> ${spin_code}"

# ── progress: stdin-driven progress bar ──────────────────────────────────────
$bin rule "progress" --color cyan
for i in {1..50}; do
    print $((i * 2))
    sleep 0.02
done | $bin progress --total 100 --label "Working" --color green --width 60

$bin rule --color cyan
$bin print "[bold green]Done.[/] See [cyan]docs/cli.md[/] for the full reference."
