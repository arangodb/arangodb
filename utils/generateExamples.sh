#!/usr/bin/env bash
export PID=$$

if test -n "$ORIGINAL_PATH"; then
    # running in cygwin...
    PS='\'
    export EXT=".exe"
else
    export EXT=""
    PS='/'
fi;

export PORT=`expr 1024 + $RANDOM`

SCRIPT="utils${PS}generateExamples.js"
LOGFILE="out${PS}log-$PID"
DBDIR="out${PS}data-$PID"

mkdir -p ${DBDIR}

echo "Database has its data in ${DBDIR}"
echo "Logfile is in ${LOGFILE}"

if [ $(uname -s) == "Darwin" ]; then
    EXEC_PATH="$(dirname "$(dirname "$0")")"
else
    EXEC_PATH="$(dirname "$(dirname "$(readlink -m "$0")")")"
fi

if [ -z "${ARANGOSH}" ];  then
    if [ -x "build/bin/arangosh${EXT}" ];  then
        ARANGOSH="build/bin/arangosh${EXT}"
        BIN_PATH=build/bin/
    elif [ -x "build/bin/RelWithDebInfo/arangosh${EXT}" ];  then
        ARANGOSH="build/bin/RelWithDebInfo/arangosh${EXT}"
        BIN_PATH=build/bin/RelWithDebInfo/
    elif [ -x "bin/arangosh${EXT}" ];  then
        ARANGOSH=bin/arangosh
        BIN_PATH=bin/
    else
        ARANGOSH="$(find "${EXEC_PATH}" -name "arangosh" -perm -001 -type f | head -n 1)"
        [ -x "${ARANGOSH}" ] || {
            echo "$0: cannot locate arangosh"
            exit 1
        }
        BIN_PATH="${ARANGOSH//arangosh/};"
    fi
fi

[ "$(uname -s)" != "Darwin" -a -x "${ARANGOSH}" ] && ARANGOSH="$(readlink -m "${ARANGOSH}")"
[ "$(uname -s)" = "Darwin" -a -x "${ARANGOSH}" ] && ARANGOSH="$(cd -P -- "$(dirname -- "${ARANGOSH}")" && pwd -P)/$(basename -- "${ARANGOSH}")"

if "${ARANGOSH}" --version | grep -q "^enterprise-version: enterprise$"; then
    ALLPROGRAMS="arangobackup arangobench arangod arangodump arangoexport arangoimport arangoinspect arangorestore arangosh"
    for HELPPROGRAM in ${ALLPROGRAMS}; do
        echo "Dumping program options of ${HELPPROGRAM}"
        "${BIN_PATH}/${HELPPROGRAM}${EXT}" --dump-options > "Documentation/Examples/${HELPPROGRAM}.json"
    done
else
    # should stop people from committing the JSON files without EE options
    echo "Skipping program option dump (requires Enterprise Edition binaries)"
fi

"${ARANGOSH}" \
    --configuration none \
    --server.endpoint tcp://127.0.0.1:${PORT} \
    --log.file ${LOGFILE} \
    --javascript.startup-directory js \
    --javascript.module-directory enterprise/js \
    --javascript.execute $SCRIPT \
    --javascript.allow-external-process-control true \
    --javascript.allow-port-testing true \
    --server.password "" \
    -- \
    "$@"

rc=$?

if test $rc -eq 0; then
    echo "removing ${LOGFILE} ${DBDIR}"
    rm -rf ${LOGFILE} ${DBDIR} arangosh.examples.js
else
    echo "failed - don't remove ${LOGFILE} ${DBDIR} - here's the logfile:"
    cat ${LOGFILE}
fi

exit $rc
