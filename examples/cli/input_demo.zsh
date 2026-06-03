#!/usr/bin/env zsh
# sparcli CLI demo: interactive input commands.
#
# Shows the shell-scripting patterns for the `sparcli` input commands:
# command substitution (UI on the terminal, value on stdout), exit-code
# handling, piping items in, and multi-select into arrays. Run it in a real
# terminal from the repository root after `make`:
#
#   ./examples/cli/input_demo.zsh
#   SPARCLI=/usr/local/bin/sparcli ./examples/cli/input_demo.zsh
#
# Every prompt can be cancelled with Esc or Ctrl-C (exit code 1).
# Full command reference: docs/cli.md

emulate -L zsh

bin=${SPARCLI:-./sparcli}
if [[ ! -x $bin ]]; then
    print -u2 "sparcli binary not found at '$bin' - run 'make' first"
    exit 1
fi

$bin rule "confirm: exit code is the answer" --color cyan
if $bin confirm "Start the demo?"; then
    $bin print "[green]Let's go.[/]"
else
    $bin print "[yellow]Skipping ahead anyway...[/]"
fi

$bin rule "input: command substitution" --color cyan
# The prompt UI is drawn on /dev/tty, only the value reaches stdout - so
# $(...) captures exactly the entered text.
if name=$($bin input "Your name:" --placeholder "Ada Lovelace"); then
    $bin print "Hello, [bold cyan]${name}[/]!"
else
    $bin print "[dim]cancelled[/]"
    name="stranger"
fi

$bin rule "input: validation via filters" --color cyan
port=$($bin input "Port:" --filter digits --max 5 --initial 8080) || port=8080
$bin print "Port: [bold]${port}[/]"

$bin rule "password: masked entry" --color cyan
secret=$($bin password "Pretend API key:" --mask "•") || secret=""
$bin print "Got [bold]${#secret}[/] characters (not echoing them, obviously)"

$bin rule "number: steps, bounds and exact decimals" --color cyan
amount=$($bin number "Amount (EUR):" --decimals 2 --decimal-sep , \
    --min 0 --max 1000 --step 0.5 --start-empty) || amount="0.00"
$bin print "Amount: [bold green]${amount}[/] EUR"

$bin rule "select: pick from arguments" --color cyan
flavor=$($bin select --prompt "Favorite flavor?" vanilla chocolate pistachio) \
    || flavor="none"
$bin print "Flavor: [bold]${flavor}[/]"

$bin rule "select: pick from a pipe (git branches)" --color cyan
if git rev-parse --git-dir > /dev/null 2>&1; then
    branch=$(git branch --format='%(refname:short)' | \
        $bin select --prompt "Check out which branch?") || branch="(none)"
    $bin print "You picked: [bold]${branch}[/] [dim](not actually checking out)[/]"
else
    $bin print "[dim]not a git repository - skipped[/]"
fi

$bin rule "select --multi: into a zsh array" --color cyan
# One selection per output line; ${(f)...} splits them into an array.
toppings=(${(f)"$($bin select --multi --prompt "Pick toppings:" \
    cheese salami mushrooms onions basil)"})
$bin print "Picked [bold]${#toppings}[/] topping(s): ${(j:, :)toppings}"

$bin rule "fuzzy: search a file list" --color cyan
file=$(find . -name '*.c' -not -path './build*' | sort | \
    $bin fuzzy --prompt "Open which file?") || file="(none)"
$bin print "File: [bold]${file}[/]"

$bin rule "fuzzy --tsv: table view" --color cyan
choice=$($bin fuzzy --tsv --header-row --prompt "Pick a tool:" << 'EOF'
Tool	Language	Purpose
rich	Python	terminal rendering
gum	Go	shell scripting prompts
sparcli	C	both of the above
EOF
) || choice="(none)"
$bin print "Tool: [bold]${choice}[/]"

$bin rule "date: calendar picker" --color cyan
when=$($bin date --prompt "Deadline?" --format "%A, %d %B %Y") || when="(none)"
$bin print "Deadline: [bold]${when}[/]"

$bin rule "textarea: multi-line text (Ctrl-D submits)" --color cyan
notes=$($bin textarea "Release notes:" --placeholder "- fixed everything") \
    || notes="(none)"
$bin print "Notes:"
print -r -- "$notes" | $bin panel --width 60

$bin rule --color cyan
$bin alert success "Demo finished. Goodbye, ${name}!"
