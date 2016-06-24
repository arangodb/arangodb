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

if [ -z "${ARANGOSH}" ];  then
  if [ -x build/bin/arangosh ];  then
    ARANGOSH=build/bin/arangosh
  elif [ -x bin/arangosh ];  then
    ARANGOSH=bin/arangosh
  else
    echo "$0: cannot locate arangosh"
  fi
fi

${ARANGOSH} \
    --configuration none \
    --server.endpoint none \
    --log.file ${LOGFILE} \
    --javascript.startup-directory js \
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
