result=$1

echo
echo "================================================================================"
echo "Result: $result"
echo "Options: $OPTS"
echo

for file in log-output/*/*/stdout; do
    base=`echo "$file" | sed -e 's:^log-output/\\([^/]*\\)/.*$:\\1:'`
    long=`echo "$file" | sed -e 's:/stdout$::'`
    short=`echo "$long" | sed -e 's:^.*_unittest *::' | sed -e 's: *--minPort.*$::' | sed -e 's:\\\\::g' | sed -e 's: *2>&1$::g' | sed -e 's:  *: :g'`

    filename="${base}_${short}.log"

    mv -- "$long/stdout" "$filename"
    rm -rf -- "$long"

    if fgrep -q "Overall state: Fail" -- "$filename"; then
        echo "FAIL: $short"
        mv -- "$filename" "log-output/FAILED_${base}_${short}.log"
    else
        mv -- "$filename" "log-output/OK_${base}_${short}.log"
    fi
done

rm -rf "log-output/${base}"

exit $result
