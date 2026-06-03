#!/usr/bin/env zsh
# sparcli example launcher.
#
# Pick an example in three steps with sparcli's own widgets - language, then
# group, then the example - and run it through `make run-example`. The launcher
# drives the sparcli CLI on its own demo tree.
#
#   ./examples/run.zsh
#   SPARCLI=/usr/local/bin/sparcli ./examples/run.zsh   # use an installed CLI
#
# Navigation: Enter selects, Esc goes back one level (Esc on the language menu
# exits). Needs the `sparcli` binary (run `make` first, or set $SPARCLI) and a
# real terminal. Full CLI reference: docs/cli.md.

emulate -L zsh
set -e

# Repo root is this script's parent dir; run make from there.
script_dir=${0:A:h}
root=${script_dir:h}

bin=${SPARCLI:-$root/sparcli}
if [[ ! -x $bin ]]; then
    print -u2 "sparcli binary not found at '$bin' - run 'make' first" \
              "(or set SPARCLI to an installed binary)."
    exit 1
fi

# Source-file extension per language category.
typeset -A ext=(c c  cpp cpp  rust rs  python py)

# Groups (immediate subdirs) of a language that contain examples, sorted.
# `cli` has no groups.
groups_of() {
    local lang=$1 d
    [[ $lang == cli ]] && return
    for d in $root/examples/$lang/*(/N); do
        [[ -n $d/**/*.${ext[$lang]}(.N) ]] && print -- ${d:t}
    done
}

# Leaf example names within a language/group, sorted. For `cli`, the demo
# scripts (group is ignored).
leaves_of() {
    local lang=$1 group=$2 files
    if [[ $lang == cli ]]; then
        files=($root/examples/cli/*.zsh(.N))
        print -l -- ${(o)${files:t}:r}
    else
        files=($root/examples/$lang/$group/*.${ext[$lang]}(.N))
        print -l -- ${(o)${files:t}:r}
    fi
}

# Languages that actually contain examples.
typeset -a languages
for lang in c cpp rust python cli; do
    if [[ $lang == cli ]]; then
        [[ -n $(leaves_of cli) ]] && languages+=(cli)
    else
        [[ -n $(groups_of $lang) ]] && languages+=($lang)
    fi
done

# Picks one leaf for a language/group and runs it; returns non-zero on Esc
# (back). On success it execs the example and never returns to the caller loop.
pick_and_run() {
    local lang=$1 group=$2 prompt=$3 example choice
    example=$(leaves_of $lang $group \
        | "$bin" fuzzy --prompt "$prompt" --accent cyan) || return 1
    [[ -z $example ]] && return 1

    if [[ $lang == cli ]]; then
        choice="cli/$example"
    else
        choice="$lang/$group/$example"
    fi
    "$bin" rule "$choice" --color green
    if [[ $lang == cli ]]; then
        SPARCLI=$bin "$root/examples/$choice.zsh"
    else
        make -C "$root" run-example EX="$choice"
    fi
    exit 0
}

"$bin" rule "sparcli examples" --color cyan

# ── Three-level picker: language -> group -> example (Esc backs out) ──────────
while true; do
    lang=$("$bin" select --prompt "Language?" --accent cyan $languages) \
        || { "$bin" alert info "Bye."; exit 0; }

    if [[ $lang == cli ]]; then
        # No group level for cli: pick a demo directly (Esc -> languages).
        pick_and_run cli "" "cli demo?" || continue
    else
        typeset -a groups=(${(f)"$(groups_of $lang)"})
        while true; do
            group=$("$bin" select --prompt "$lang group?" --accent cyan \
                $groups) || break                   # Esc -> back to languages
            pick_and_run $lang $group "$lang/$group example?" || continue
        done
    fi
done
