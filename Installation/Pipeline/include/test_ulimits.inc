ulimit -n 4096 || exit 1
ulimit -c unlimited || exit 1

echo "LIMIT FD: `ulimit -n`"
echo "CORE LIMIT: `ulimit -c`"

