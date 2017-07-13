rm -rf core.* *.log out
rm -rf tmp && mkdir tmp
export TMPDIR=$(pwd)/tmp
export TEMPDIR=$(pwd)/tmp

rm -rf "log-output"
mkdir -p "log-output"

echo "TMPDIR: $TMPDIR"
