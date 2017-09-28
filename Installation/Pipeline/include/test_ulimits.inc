ulimit -n 4096 || exit 1
ulimit -c unlimited || exit 1

if [ "$os" == mac ]; then
    sudo sysctl -w kern.corefile=core.%P
fi

echo "LIMIT FD: `ulimit -n`"
echo "CORE LIMIT: `ulimit -c`"

