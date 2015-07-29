#!/bin/bash

if [[ "$1" == "" ]]; then
  echo "usage $0 outfile"
  exit 1
fi
out="$1"

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
      # completions="`\"$executable\" $command | grep -o \"^\\ \\+--[a-z-]\\+\\(\\.[a-z0-9-]\\+\\)\\?\" | sed -e \"s/^/complete --command $progname -a /g\"`"
      echo "# completions for $progname" >> "$out"

      completions="`\"$executable\" $command`"
      (echo "$completions" | grep "^  " | awk '/^  --/{if (x)print x;x="";}{x=(!x)?$0:x" "$0;}END{print x;}' | sed -e "s/ \+/ /g" | sed -e "s/(default:.*)//g" | sed -e "s/(current:.*)//g" | sed -e "s/<[a-zA-Z0-9]\+>//g" | tr -d "'" | sed -e "s/^ \+--\([a-zA-Z0-9.\-]\+\) \+\(.\+\)$/complete --command $progname -l '\\1' -d '\\2'/g" | sed -e "s/ \+'$/'/g") >> "$out"

      echo "" >> "$out"
    fi
  done

echo "completions stored in file $out"
echo "now copy this file to /etc/bash_completion.d/arangodb"
