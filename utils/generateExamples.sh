#!/bin/bash
export PID=$$

if test -n "$ORIGINAL_PATH"; then
    # running in cygwin...
    PS='\'
    export EXT=".exe"
else
    export EXT=""
    PS='/'
fi;

SCRIPT="utils${PS}generateExamples.js"
LOGFILE="out${PS}log-$PID"
DBDIR="out${PS}data-$PID"

mkdir -p ${DBDIR}

echo Database has its data in ${DBDIR}
echo Logfile is in ${LOGFILE}

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


ALLPROGRAMS="arangobench arangod arangodump arangoexport arangoimport arangorestore arangosh"

for HELPPROGRAM in ${ALLPROGRAMS}; do
    "${BIN_PATH}/${HELPPROGRAM}${EXT}" --dump-options > "Documentation/Examples/${HELPPROGRAM}.json"
done

"${ARANGOSH}" \
    --configuration none \
    --server.endpoint none \
    --log.file ${LOGFILE} \
    --javascript.startup-directory js \
    --javascript.module-directory enterprise/js \
    --javascript.execute $SCRIPT \
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
