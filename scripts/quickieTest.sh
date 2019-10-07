#!/usr/bin/env bash
JSLINTOUT=/tmp/jslintout.$$
trap "rm -rf ${JSLINTOUT}" EXIT

./utils/jslint.sh 2>&1 > ${JSLINTOUT} &
JSLINTPID="$!"

scripts/unittest shell_server --test tests/js/common/shell/shell-quickie.js "$@"
scripts/unittest shell_server --test tests/js/common/shell/shell-quickie.js --cluster true --agencySupervision true "$@"
scripts/unittest shell_client --test tests/js/common/shell/shell-quickie.js "$@"
scripts/unittest shell_client --test tests/js/common/shell/shell-quickie.js --cluster true --agencySize 1 "$@"

echo "waiting for the result of jslint..."

if wait $!; then
    echo "jslint test passed!"
else
    cat ${JSLINTOUT}
    echo "jslint test failed!"
    exit 1
fi
