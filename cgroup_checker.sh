#!/bin/sh


echo "Check cgroup version"
cat /proc/mounts | grep cgroup

PROC_ID="$(echo $$)"
echo "Proc id: $PROC_ID"

GROUP_NAME=$(cat "/proc/$PROC_ID/cgroup" | cut -d'/' -f 2)
echo "Group name: $GROUP_NAME"

# Store the directory path
directory="/sys/fs/cgroup/$GROUP_NAME"

# Check if the provided path is a directory
if ! [ -d "$directory" ]; then
  echo "Error: '$directory' is not a valid directory"
  exit 1
fi

# Loop through all files in the directory
find "$directory" -type f | while IFS= read -r file; do
  # Print the filename
  echo "--- Contents of: $file ---"

  # Print the file contents
  cat "$file"

  # Add a separator for readability (optional)
  echo ""
done
