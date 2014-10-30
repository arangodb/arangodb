#!/bin/bash

echo "$0: checking for core file"
if [[ -f core ]]; then
  echo "$0: found a core file"
  sudo echo "thread apply all bt full" | gdb -c core bin/arangod
fi

echo "$0: done"
