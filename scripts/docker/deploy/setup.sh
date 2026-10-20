#!/bin/sh
#getent group arangodb > /dev/null || addgroup -S arangodb
#getent passwd arangodb > /dev/null || adduser -S -G arangodb -D -h /usr/share/arangodb3 -H -s /bin/false -g "ArangoDB Application User" arangodb

install -o root -g root -m 755 -d /var/lib/arangodb3
install -o root -g root -m 755 -d /var/lib/arangodb3-apps
# Note that the log dir is 777 such that any user can log there.
install -o root -g root -m 777 -d /var/log/arangodb3

mkdir /docker-entrypoint-initdb.d/

# Bind to all endpoints (in the container):
sed -i -e 's~^endpoint.*8529$~endpoint = tcp://0.0.0.0:8529~' /etc/arangodb3/arangod.conf
# Remove the uid setting in the config file, since we want to be able
# to run as an arbitrary user:
sed -i \
    -e 's!^\(file\s*=\s*\).*!\1 -!' \
    -e 's~^uid = .*$~~' \
    /etc/arangodb3/arangod.conf

