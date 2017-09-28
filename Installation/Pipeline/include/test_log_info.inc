test -z "$type" || echo "TYPE: $type"
test -z "$foxx" || echo "FOXX: $foxx"
echo "ENGINE: $engine"
test -z "$concurrency" || echo "CONCURRENY: $concurrency"
echo "HOST: `hostname`"
echo "PWD: `pwd`"

if [ "$os" == linux ]; then
    echo "CORE PATTERN: `cat /proc/sys/kernel/core_pattern`"
fi

echo
echo "ARANGOD VERSION: `build/bin/arangod --version`"
