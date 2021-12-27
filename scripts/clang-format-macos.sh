#!/usr/bin/env bash
# pre-requisite: MacOS-based system
# pre-requisite: Docker

# Get root directory of repo (can be passed as argument)
[[ -z "$1" ]] && adb_path="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )" || adb_path=$(echo "$1" | sed 's:/*$::')
ent_dir="enterprise"

echo "ArangoDB directory: $adb_path"
echo "Enterprise directory: $adb_path/$ent_dir"

cd "$adb_path"
community_diff=$(git diff HEAD --diff-filter=ACMR --name-only -- '*.cpp' '*.hpp' '*.cc' '*.c' '*.h')

if [ -d "$adb_path/$ent_dir" ]
then
   cd "$adb_path/$ent_dir"
   enterprise_diff=$(git diff HEAD --diff-filter=ACMR --name-only -- '*.cpp' '*.hpp' '*.cc' '*.c' '*.h' | sed "s,^,$ent_dir/,")
fi

cd "$adb_path"
diff="$community_diff $enterprise_diff"
if ! [[ -z "${diff// }" ]]; then
  docker run --rm -u "$(id -u):$(id -g)" --mount type=bind,source="$adb_path",target=/usr/src/arangodb arangodb/clang-format:1.0 "$diff"
fi

echo "(done)"
