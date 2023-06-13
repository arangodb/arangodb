#!/bin/bash

# Generates a stand-alone bash script which installs completions for
# `./scripts/unittest`.
# Save it like
#   ./scripts/generateUnittestCompletion.bash > ~/arango_unittest_comp.bash
# and run
#   . ~/arango_unittest_comp.bash
# to install completions, probably in your .bashrc.
#
# You can also install completions directly by running
#   eval "$(./scripts/generateUnittestCompletion.bash)"
# in your shell, or .bashrc, but note that it has to be executed in the arangodb
# directory, so ./scripts/unittest can be found.

test -e ./scripts/unittest || exit 1

cat <<'PRINT'
_unittest_complete()
{
PRINT

cat <<PRINT
    COMPINFO=$(printf '%q' "$(./scripts/unittest --dump-completions 2> /dev/null)")
PRINT

cat <<'PRINT'
    local cur prev
    COMPREPLY=()
    prev=("${COMP_WORDS[@]:1:COMP_CWORD-1}")
    cur="${COMP_WORDS[COMP_CWORD]}"

    if [ "$COMP_CWORD" = "1" ]; then
        SUITES=( $( jq -r '.suites[]' <<<"$COMPINFO" ) )
        COMPREPLY=( $(compgen -W "${SUITES[*]}" -- "${cur}" ) )
    else
        OPTIONS=( $( jq -r '.options[]' <<<"$COMPINFO" ) )
        COMPREPLY=( $(compgen -W "${OPTIONS[*]}" -- "${cur}" ) )
    fi

    return 0
}

complete -F _unittest_complete ./scripts/unittest
PRINT
