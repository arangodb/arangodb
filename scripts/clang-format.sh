#!/usr/bin/env bash
[[ -z "$1" ]] && adb_path="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )" || adb_path=$(echo "$1" | sed 's:/*$::') # Get root directory of repo (can be passed as argument)
[[ -z "$2" ]] && ent_dir="enterprise" || ent_dir=$(echo "$2" | sed 's:/*$::') # Get enterprise directory (can be passed as argument)

echo "ArangoDB directory: $adb_path"
echo "Enterprise directory: $adb_path/$ent_dir"

cd "$adb_path"
community_diff=$(git diff HEAD --diff-filter=ACMR --name-only -- '*.cpp' '*.hpp' '*.cc' '*.c' '*.h')

if [ -d "$adb_path/$ent_dir" ] # assume enterprise directory is within arangodb directory
then
   cd "$adb_path/$ent_dir"
   enterprise_diff=$(git diff HEAD --diff-filter=ACMR --name-only -- '*.cpp' '*.hpp' '*.cc' '*.c' '*.h' | sed "s,^,$ent_dir/,")
fi

diff="$community_diff $enterprise_diff"
if ! [[ -z "${diff// }" ]]; then
  docker run --rm -v "$adb_path":/usr/src/arangodb arangodb/clang-format:latest "$diff"
fi

echo "(done)"