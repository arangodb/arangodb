echo "TYPE: $type"
echo "ENGINE: $engine"
echo "CONCURRENY: $concurrency"
echo "HOST: `hostname`"
echo "PWD: `pwd`"
echo "CORE PATTERN: `cat /proc/sys/kernel/core_pattern`"
echo "CORE LIMIT: `ulimit -c`"

echo
echo "ARANGOD VERSION: `build/bin/arangod --version`"
