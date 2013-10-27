#!/bin/bash

ARANGO_ROOT="`echo $0 | sed -e 's/\/Contents\/MacOS\/ArangoDB-CLI//'`"

# create start script

SCRIPTS="`( cd $ARANGO_ROOT/Contents/MacOS/opt/arangodb && ls -1 {bin,sbin}/* )`"

for script in $SCRIPTS; do
  base="`basename $script`"

  (
    echo "#!/bin/bash"
    echo
    echo "export ARANGO_ROOT=\"${ARANGO_ROOT}/Contents/MacOS//opt/arangodb\""
    echo "export DATABASEDIR=\"${ARANGO_ROOT}/Contents/MacOS//opt/arangodb/var/lib/arangodb\""
    echo "export LOGDIR=\"${ARANGO_ROOT}/Contents/MacOS//opt/arangodb/var/log/arangodb\""
    echo "export PKGDATADIR=\"${ARANGO_ROOT}/Contents/MacOS//opt/arangodb/share/arangodb\""
    echo
    echo "exec \"${ARANGO_ROOT}/Contents/MacOS/opt/arangodb/$script\" -c \"${ARANGO_ROOT}/Contents/MacOS/opt/arangodb/etc/arangodb/${base}-relative.conf\" \$*"
  ) > ${ARANGO_ROOT}/Contents/MacOS/$base.$$

  chmod 755 ${ARANGO_ROOT}/Contents/MacOS/$base.$$
  mv ${ARANGO_ROOT}/Contents/MacOS/$base.$$ ${ARANGO_ROOT}/Contents/MacOS/$base
done

# start the server

PIDFILE="${ARANGO_ROOT}/Contents/MacOS/opt/arangodb/var/run/arangod.pid"

if [ -f "${PIDFILE}" ];  then
result=`
/usr/bin/osascript -s so <<-EOF
tell application "System Events"
  activate
  display dialog "PID File ${PIDFILE} exists, server already running. Press OK to try to start the server anyhow."
end tell
EOF
`

  if echo $result | fgrep -q "User canceled";  then
    exit 0
  fi
fi

test -d "${ARANGO_ROOT}/Contents/MacOS/opt/arangodb/var/run" || mkdir "${ARANGO_ROOT}/Contents/MacOS/opt/arangodb/var/run"
${ARANGO_ROOT}/Contents/MacOS/arangod --daemon --pid-file "${PIDFILE}"

# create some information for the user

INFOFILE="/tmp/ArangoDB-CLI.info.$$"

(
  echo "ArangoDB server has been started"
  echo ""
  echo "The database directory is located at"
  echo "   '${ARANGO_ROOT}/Contents/MacOS/opt/arangodb/var/lib/arangodb'"
  echo ""
  echo "The log file is located at"
  echo "   '${ARANGO_ROOT}/Contents/MacOS/opt/arangodb/var/log/arangodb/arangod.log'"
  echo ""
  echo "You can access the server using a browser at 'http://127.0.0.1:8529/'"
  echo "or start the ArangoDB shell"
  echo "   '${ARANGO_ROOT}/Contents/MacOS/arangosh'"
  echo ""
  echo "Switching to log-file now, killing this windows will NOT stop the server."
  echo ""
  echo ""
) > $INFOFILE

# start a Terminal.app session

/usr/bin/osascript <<-EOF
tell application "Terminal"
  activate
  do script "clear && cat $INFOFILE && rm $INFOFILE && sleep 20 && exec tail -1 -f ${ARANGO_ROOT}/Contents/MacOS/opt/arangodb/var/log/arangodb/arangod.log"
end tell
EOF
