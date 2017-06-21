rm -rf core.* *.log out
rm -rf tmp && mkdir tmp
export TMPDIR=$(pwd)/tmp
export TEMPDIR=$(pwd)/tmp

mkdir -p log-output
rm -rf log-output/$type
