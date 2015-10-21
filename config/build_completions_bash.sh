#!/bin/bash

if [[ "$1" == "" ]]; then
  echo "usage $0 outfile"
  exit 1
fi
out="$1"

rm -f completions-template

cat << 'EOF' > completions-template
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

for progname in arangob arangosh arangoimp arangodump arangorestore arangod
  do
    # find the executable
    # check if the executable exists
    if [[ -f "bin/$progname" ]]; then
      executable="bin/$progname"
  
      # setup the help command string
      command="--help" 
      if [ "x$progname" == "xarangod" ]; then
        command="--help-all"
      fi

      # set up the list of completions for the executable
      completions="`\"$executable\" $command | grep -o \"^\\ \\+--[a-z-]\\+\\(\\.[a-z0-9-]\\+\\)\\?\" | xargs`"

      sed -e "s/PROGNAME/$progname/g" -e "s/PROGOPTS/$completions/g" completions-template >> $out          
    fi
  done

rm -f completions-template

echo "completions stored in file $out"
echo "now copy this file to /etc/bash_completion.d/arangodb"
