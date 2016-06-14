#!/bin/bash

ROOTDIR="`echo $0 | sed -e 's:/Contents/MacOS/ArangoDB-CLI::'`"

# create start script

SCRIPTS="`( cd ${ROOTDIR}/Contents/MacOS/opt/arangodb && ls -1 {bin,sbin}/* )`"

for script in $SCRIPTS; do
  base="`basename $script`"

  (
    echo "#!/bin/bash"
    echo
    echo "export ROOTDIR=\"${ROOTDIR}/Contents/MacOS/opt/arangodb\""
    echo

    echo "exec \"\${ROOTDIR}/$script\" -c \"\${ROOTDIR}/etc/arangodb3/${base}.conf\" \$*"
  ) > ${ROOTDIR}/Contents/MacOS/$base.$$

  chmod 755 ${ROOTDIR}/Contents/MacOS/$base.$$
  mv ${ROOTDIR}/Contents/MacOS/$base.$$ ${ROOTDIR}/Contents/MacOS/$base
done

# start the server

PIDFILE="${ROOTDIR}/Contents/MacOS/opt/arangodb/var/run/arangod.pid"

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

test -d "${ROOTDIR}/Contents/MacOS/opt/arangodb/var/run" || mkdir "${ROOTDIR}/Contents/MacOS/opt/arangodb/var/run"
${ROOTDIR}/Contents/MacOS/arangod --daemon --pid-file "${PIDFILE}"

# create some information for the user

INFOFILE="/tmp/ArangoDB-CLI.info.$$"

(
  echo "ArangoDB server has been started"
  echo ""
  echo "The database directory is located at"
  echo "   '${ROOTDIR}/Contents/MacOS/opt/arangodb/var/lib/arangodb3'"
  echo ""
  echo "The log file is located at"
  echo "   '${ROOTDIR}/Contents/MacOS/opt/arangodb/var/log/arangodb3/arangod.log'"
  echo ""
  echo "You can access the server using a browser at 'http://127.0.0.1:8529/'"
  echo "or start the ArangoDB shell"
  echo "   '${ROOTDIR}/Contents/MacOS/arangosh'"
  echo ""
  echo "Switching to log-file now, killing this windows will NOT stop the server."
  echo ""
  echo ""
) > $INFOFILE

# start a Terminal.app session

/usr/bin/osascript <<-EOF
tell application "Terminal"
  activate
  do script "clear && cat $INFOFILE && rm $INFOFILE && sleep 20 && exec tail -1 -f ${ROOTDIR}/Contents/MacOS/opt/arangodb/var/log/arangodb3/arangod.log"
end tell
EOF
