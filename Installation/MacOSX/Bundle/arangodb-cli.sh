#!/bin/bash

ARANGODB_ROOT="`echo $0 | sed -e 's/\/Contents\/MacOS\/ArangoDB-CLI//'`/"

# create start script

SCRIPTS="`( cd ${ARANGODB_ROOT}Contents/MacOS/opt/arangodb && ls -1 {bin,sbin}/* )`"

for script in $SCRIPTS; do
  base="`basename $script`"

  (
    echo "#!/bin/bash"
    echo
    echo "export ARANGODB_ROOT=\"${ARANGODB_ROOT}Contents/MacOS/\""
    echo "export LOCALSTATEDIR=\"\${ARANGODB_ROOT}opt/arangodb/var\""
    echo "export PKGDATADIR=\"\${ARANGODB_ROOT}opt/arangodb/share/arangodb\""
    echo
    if [ "$base" == "arango-dfdb" ];  then
      echo "exec \"\${ARANGODB_ROOT}opt/arangodb/$script\" \$*"
    elif [ "$base" == "foxx-manager" ];  then
      echo "exec \"\${ARANGODB_ROOT}opt/arangodb/$script\" -c \"\${ARANGODB_ROOT}opt/arangodb/etc/arangodb/arangosh-relative.conf\" \$*"
    else
      echo "exec \"\${ARANGODB_ROOT}opt/arangodb/$script\" -c \"\${ARANGODB_ROOT}opt/arangodb/etc/arangodb/${base}-relative.conf\" \$*"
    fi
  ) > ${ARANGODB_ROOT}Contents/MacOS/$base.$$

  chmod 755 ${ARANGODB_ROOT}Contents/MacOS/$base.$$
  mv ${ARANGODB_ROOT}Contents/MacOS/$base.$$ ${ARANGODB_ROOT}Contents/MacOS/$base
done

# start the server

PIDFILE="${ARANGODB_ROOT}Contents/MacOS/opt/arangodb/var/run/arangod.pid"

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

test -d "${ARANGODB_ROOT}Contents/MacOS/opt/arangodb/var/run" || mkdir "${ARANGODB_ROOT}Contents/MacOS/opt/arangodb/var/run"
${ARANGODB_ROOT}Contents/MacOS/arangod --daemon --pid-file "${PIDFILE}"

# create some information for the user

INFOFILE="/tmp/ArangoDB-CLI.info.$$"

(
  echo "ArangoDB server has been started"
  echo ""
  echo "The database directory is located at"
  echo "   '${ARANGODB_ROOT}Contents/MacOS/opt/arangodb/var/lib/arangodb'"
  echo ""
  echo "The log file is located at"
  echo "   '${ARANGODB_ROOT}Contents/MacOS/opt/arangodb/var/log/arangodb/arangod.log'"
  echo ""
  echo "You can access the server using a browser at 'http://127.0.0.1:8529/'"
  echo "or start the ArangoDB shell"
  echo "   '${ARANGODB_ROOT}Contents/MacOS/arangosh'"
  echo ""
  echo "Switching to log-file now, killing this windows will NOT stop the server."
  echo ""
  echo ""
) > $INFOFILE

# start a Terminal.app session

/usr/bin/osascript <<-EOF
tell application "Terminal"
  activate
  do script "clear && cat $INFOFILE && rm $INFOFILE && sleep 20 && exec tail -1 -f ${ARANGODB_ROOT}Contents/MacOS/opt/arangodb/var/log/arangodb/arangod.log"
end tell
EOF
