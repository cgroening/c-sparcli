#!/bin/sh
# Anti-drift check for the zsh completion (completions/_sparcli).
#
# The CLI uses hand-written usage/getopt parsing and a hand-written zsh
# completion (the args parser migration is deliberately deferred, see the
# README). To keep the completion from silently falling behind the binary,
# this test asserts that every long option each `sparcli <cmd> --help`
# documents is also offered by the completion - resolving the shared
# `$common_opts` / `$input_opts` / `$box_opts` arrays the per-command specs
# splat in. Globals handled once at the top-level `_arguments` are exempt.
#
# Usage: test_completion.sh ./sparcli
set -u

BIN=$1
HERE=$(dirname "$0")
COMP=$HERE/../../completions/_sparcli

# Options offered at the top-level `_arguments` (not per command).
GLOBALS=" help no-color no-markup allow-ansi version "

# Every `sparcli` subcommand.
CMDS="print panel rule table list tree kv alert badge columns spin progress \
confirm input password number textarea select fuzzy date"

# Extracts bare long-option names (no leading --) from stdin, sorted/unique.
flags() {
    grep -oE -- '--[a-z][a-z0-9-]+' | sed 's/^--//' | sort -u
}

# Long options literally listed in a `local -a NAME=( ... )` array definition.
array_flags() {
    awk -v a="$1" '
        $0 ~ ("local -a " a "=\\(") { f = 1 }
        f { print }
        f && /^[[:space:]]*\)/ { exit }
    ' "$COMP" | flags
}

COMMON=$(array_flags common_opts)
INPUT=$(array_flags input_opts)
BOX=$(array_flags box_opts)

fail=0
for c in $CMDS; do
    help_flags=$("$BIN" "$c" --help 2>&1 | flags)

    # The command's case block: from `<cmd>)` to the closing `;;`.
    block=$(awk -v c="$c" '
        $0 ~ ("^[[:space:]]*" c "\\)") { f = 1 }
        f { print }
        f && /;;/ { exit }
    ' "$COMP")

    # Effective completion flags = literal flags in the block + the flags of
    # any shared array it splats ($input_opts itself includes $common_opts).
    eff=$(printf '%s\n' "$block" | flags)
    case $block in
        *'$common_opts'*) eff="$eff
$COMMON" ;;
    esac
    case $block in
        *'$input_opts'*) eff="$eff
$INPUT
$COMMON" ;;
    esac
    case $block in
        *'$box_opts'*) eff="$eff
$BOX" ;;
    esac
    eff=$(printf '%s\n' "$eff" | sort -u)

    for f in $help_flags; do
        case $GLOBALS in *" $f "*) continue ;; esac
        if ! printf '%s\n' "$eff" | grep -qx "$f"; then
            printf '  MISSING in completion: %s --%s\n' "$c" "$f"
            fail=1
        fi
    done
done

if [ "$fail" -ne 0 ]; then
    echo "completion drift: _sparcli is missing flags listed above" >&2
    echo "update completions/_sparcli, then rerun make test-cli-completion" >&2
    exit 1
fi
echo "completion: every documented flag is offered by _sparcli"
exit 0
