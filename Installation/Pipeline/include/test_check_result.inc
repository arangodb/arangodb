result=$1

echo
echo "================================================================================"
echo "Result: $result"
echo "Options: $OPTS"
echo

for file in log-output/*/*/stdout; do
    base=`echo "$file" | sed -e 's:^\\(log-output/[^/]*\\)/.*$:\\1:'`
    long=`echo "$file" | sed -e 's:/stdout$::'`
    short=`echo "$long" | sed -e 's:^.*_unittest *::' | sed -e 's: *--minPort.*$::' | sed -e 's:\\\\::g' | sed -e 's: *2>&1$::g' | sed -e 's:  *: :g'`
    failed=0

    filename="$base/$short"

    mv -- "$long/stdout" "$filename"
    rm -rf -- "$long"

    if fgrep -q "Overall state: Fail" -- "$filename"; then
        failed=1
        echo "FAIL: $short"
        mv -- "$filename" "${base}/FAILED_${short}.log"
    else
        mv -- "$filename" "${base}/OK_${short}.log"
    fi

    if test "$failed" -eq 0; then
        mv "${base}" "OK_${base}"
    else
        mv "${base}" "FAILED_${base}"
    fi
done

exit $result
