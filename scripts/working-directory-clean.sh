#!/bin/sh

# A script to check that the working tree is clean, and otherwise
# store diffs. Mostly of use for CI

check_repo() {
  REPO=$1
  DIFFOUT=$2

  if [ ! -d "$REPO" ]; then
    echo "Working directory \"$REPO\" does not exist"
    exit 2
  fi

  if [ "$DIFFOUT" != "" ] && [ ! -d "$DIFFOUT" ]; then
    echo "Requested output directory \"$DIFFOUT\" does not exist or is not a directory"
    exit 3
  fi

  if output=$(git -C $REPO status --porcelain) && [ -z "$output" ]; then
    # Working directory clean
    echo "Working directory $REPO is clean"
    exit 0
  else
    echo "Working directory $REPO is NOT clean"
    echo "$output"
    if [ "$DIFFOUT" != "" ]; then
      echo "Writing to $DIFFOUT"
      echo "$output" > $DIFFOUT/status
      git -C $REPO diff > $DIFFOUT/diff
    fi
    exit 1
  fi
}

check_repo $1 $2
