#!/bin/sh
install -o root -g root -m 755 -d /var/lib/arangodb3
install -o root -g root -m 755 -d /var/lib/arangodb3-apps
# Note that the log dir is 777 such that any user can log there.
install -o root -g root -m 777 -d /var/log/arangodb3

mkdir /docker-entrypoint-initdb.d/

# - bind to all endpoints (in the container):
# - remove the uid setting in the config file, since we want to be able
#   to run as an arbitrary user:
sed -i \
    -e 's~^endpoint.*8529$~endpoint = tcp://0.0.0.0:8529~'  \
    -e 's~^uid = .*$~~' \
    -e 's!^\(file\s*=\s*\).*!\1 -!' \
    /etc/arangodb3/arangod.conf
