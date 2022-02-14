#!/usr/bin/env bash
# pre-requisite: Linux-based system
# pre-requisite: Docker
# pre-requisite: Bash 4
# pre-requisite: Git 2.9+
# pre-requisite: sed

adb_path="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

ent_dir="enterprise"

echo "ArangoDB directory: $adb_path"
pushd "$adb_path" > /dev/null

changed_files_filename=".clang-format-$$.changed.tmp"
# clean up after ourselves
trap "rm -f $changed_files_filename" EXIT SIGINT SIGTERM SIGHUP

git diff --diff-filter=ACMRT --name-only -- arangod/ lib/ client-tools/ tests/ | grep -e '\.ipp$' -e '\.tpp$' -e '\.cpp$' -e '\.hpp$' -e '\.cc$' -e '\.c$' -e '\.h$' > "$changed_files_filename"

if [ -d "$ent_dir" ]
then
  # collect changes from enterprise repository as well
  echo "Enterprise directory: $adb_path/$ent_dir"
  pushd "$ent_dir" > /dev/null
  git diff --diff-filter=ACMRT --name-only -- Enterprise tests/ | grep -e '\.ipp$' -e '\.tpp$' -e '\.cpp$' -e '\.hpp$' -e '\.cc$' -e '\.c$' -e '\.h$' | sed -e "s/^/$ent_dir\//g" >> "../$changed_files_filename"
  popd > /dev/null
fi

if [ -s "$changed_files_filename" ]; then
  docker run --rm -u "$(id -u):$(id -g)" --mount type=bind,source="$adb_path",target=/usr/src/arangodb jsteemann/clang-format-docker:0.2 "format" "$changed_files_filename" 
fi
status=$?

popd > /dev/null

exit $status
