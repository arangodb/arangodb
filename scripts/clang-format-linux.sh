#!/usr/bin/env bash
# pre-requisite: Linux-based system
# pre-requisite: Docker
# pre-requisite: Bash 4
# pre-requisite: Git 2.9+
# pre-requisite: sed
# pre-requisite: grep
# pre-requisite: cat

# when invoking this script without any arguments, it will apply formatting for all
# locally modified files that are tracked by git.
# when invoking the script with arguments, each argument must be the name of a
# file to check/reformat.
adb_path="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

echo "ArangoDB directory: $adb_path"
pushd "$adb_path" > /dev/null

changed_files_filename=".clang-format-$$.changed.tmp"
# clean up after ourselves
trap "rm -f $changed_files_filename" EXIT SIGINT SIGTERM SIGHUP

if [[ "$#" -gt 0 ]]
then
  # arguments given
  for file in $@
  do 
    echo "$file" >> $changed_files_filename
  done
else
  # no arguments given
  git diff --diff-filter=ACMRT --name-only -- arangod/ lib/ client-tools/ tests/ | grep -e '\.ipp$' -e '\.tpp$' -e '\.cpp$' -e '\.hpp$' -e '\.cc$' -e '\.c$' -e '\.h$' > "$changed_files_filename"

  ent_dir="enterprise"
  if [ -d "$ent_dir" ]
  then
    # collect changes from enterprise repository as well
    echo "Enterprise directory: $adb_path/$ent_dir"
    pushd "$ent_dir" > /dev/null
    git diff --diff-filter=ACMRT --name-only -- Enterprise tests/ | grep -e '\.ipp$' -e '\.tpp$' -e '\.cpp$' -e '\.hpp$' -e '\.cc$' -e '\.c$' -e '\.h$' | sed -e "s/^/$ent_dir\//g" >> "../$changed_files_filename"
    popd > /dev/null
  fi
fi

if [ -s "$changed_files_filename" ]; then
  echo "About to run formatting on the following files:"
  cat -n "$changed_files_filename"

  docker run --rm -u "$(id -u):$(id -g)" --mount type=bind,source="$adb_path",target=/usr/src/arangodb jsteemann/clang-format-docker:0.3 "format" "$changed_files_filename" 
fi
status=$?

popd > /dev/null

exit $status
