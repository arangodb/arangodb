#!/usr/bin/env bash
# pre-requisite: Linux-based system
# pre-requisite: Docker
# pre-requisite: Bash 4
# pre-requisite: Git 2.9+
# pre-requisite: sed
# pre-requisite: cat

# when invoking this script without any arguments, it will apply formatting for all
# locally modified files that are tracked by git.
# when invoking the script with arguments, each argument must be the name of a
# file or directory to check/reformat.
adb_path="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

echo "ArangoDB directory: $adb_path"
pushd "$adb_path" > /dev/null

changed_files_filename=".clang-format-$$.changed.tmp"
# clean up after ourselves
trap "rm -f $changed_files_filename $changed_files_filename.sorted" EXIT
trap "rm -f $changed_files_filename $changed_files_filename.sorted; exit" SIGINT SIGTERM SIGHUP

if [[ "$#" -gt 0 ]]
then
  # arguments given; directories are expanded to all files below them
  find "$@" -type f > "$changed_files_filename"
else
  # no arguments given: all locally modified files, staged or not
  git diff --diff-filter=ACMRT --name-only HEAD > "$changed_files_filename"

  ent_dir="enterprise"
  if [ -d "$ent_dir" ]
  then
    # collect changes from enterprise repository as well
    echo "Enterprise directory: $adb_path/$ent_dir"
    git -C "$ent_dir" diff --diff-filter=ACMRT --name-only HEAD | sed -e "s/^/$ent_dir\//g" >> "$changed_files_filename"
  fi
fi

# keep only C/C++ sources, as marked in .gitattributes; enterprise paths are matched by
# the same patterns
sort -u "$changed_files_filename" | git check-attr --stdin clang-format | sed -n 's/: clang-format: set$//p' > "$changed_files_filename.sorted"

if [ -s "$changed_files_filename.sorted" ]; then

  echo
  echo "About to run formatting on the following files:"
  cat -n "$changed_files_filename.sorted"
  echo

  docker run --rm -u "$(id -u):$(id -g)" --mount type=bind,source="$adb_path",target=/usr/src/arangodb --security-opt label=disable arangodb/clang-format:22.1.3 "format" "$changed_files_filename.sorted"
fi
status=$?

popd > /dev/null

exit $status
