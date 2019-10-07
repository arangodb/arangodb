#!/usr/bin/env bash

if [[ "$1" == "" ]]; then
  echo "usage $0 outfile"
  exit 1
fi
out="$1"

if [ ! -d "build/bin" ]; then
  echo "expecting directory build/bin to be present"
  exit 1
fi

rm -f /tmp/completions-template

cat << 'EOF' > /tmp/completions-template
_PROGNAME()
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    opts="PROGOPTS"

    if [[ ${cur} == -* ]] ; then
        COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
        return 0
    fi
}

complete -o default -F _PROGNAME PROGNAME

EOF


echo "" > $out

progs="arangobench arangosh arangoimport arangodump arangorestore arangod"

for progname in $progs
  do
    # find the executable
    # check if the executable exists
    if [[ -f "build/bin/$progname" ]]; then
      executable="build/bin/$progname"

      # setup the help command string
      command="--help-all"

      # set up the list of completions for the executable
      completions="`\"$executable\" $command | grep -o \"^\\ \\+--[a-z-]\\+\\(\\.[a-z0-9-]\\+\\)\\?\" | xargs`"

      sed -e "s/PROGNAME/$progname/g" -e "s/PROGOPTS/$completions/g" /tmp/completions-template >> $out
    fi
  done

rm -f /tmp/completions-template

echo "completions stored in file $out"
echo "now copy this file to /etc/bash_completion.d/arangodb"
