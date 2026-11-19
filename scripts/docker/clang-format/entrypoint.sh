#!/bin/sh
# Usage: entrypoint.sh format|validate <file with one path per line>
# Paths are relative to the repository root, which is mounted at /usr/src/arangodb.
cd arangodb

operation="$1"
changed_filenames_file="$2"

case "$operation" in
  format|validate) ;;
  *) echo "unknown operation type: $operation"; exit 2 ;;
esac

clang-format --version

checked=$(wc -l < "$changed_filenames_file")
echo "checking $checked file(s)..."
formatted=0
passed=0
while read -r file
do
  if [ "$operation" = "validate" ]
  then
    # check the staged version, because that is what gets committed
    git show ":0:$file" | clang-format -Werror -ferror-limit=0 --dry-run --assume-filename="$file" -style=file
    status=$?
    if [ $status -eq 0 ]
    then
      passed=$((passed+1))
    elif [ $status -eq 1 ]
    then
      echo "file needs reformatting: $file"
      echo
      formatted=$((formatted+1))
    else
      echo "unknown error formatting file: $file"
      exit 2
    fi
  else
    if clang-format -Werror --dry-run -style=file "$file" 2> /dev/null
    then
      passed=$((passed+1))
    elif clang-format -i -style=file "$file"
    then
      echo "reformatting $file"
      formatted=$((formatted+1))
    else
      echo "error formatting $file"
      exit 1
    fi
  fi
done < "$changed_filenames_file"

echo

if [ "$operation" = "format" ]
then
  echo "done - $checked file(s) checked, $passed file(s) passed, $formatted file(s) reformatted"
elif [ $formatted -ne 0 ]
then
  echo "erroring out because $formatted file(s) still need(s) to be reformatted!!!"
  echo
  exit 1
fi
