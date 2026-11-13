#!/bin/bash
# Waits for all brokers listed in SCHEMA_REGISTRY_KAFKASTORE_BOOTSTRAP_SERVERS
# (e.g. PLAINTEXT://kafka-1:19092,PLAINTEXT://kafka-2:29092) to accept TCP
# connections, then hands over to the original schema registry entrypoint.
set -e

port_open() {
    if command -v nc > /dev/null 2>&1; then
        nc -z "$1" "$2" > /dev/null 2>&1
    else
        # the base image doesn't necessarily ship netcat; bash can do it too
        timeout 2 bash -c "exec 3<>/dev/tcp/$1/$2" > /dev/null 2>&1
    fi
}

wait_server() {
    local count=0
    echo -n "waiting for $1:$2 "
    until port_open "$1" "$2"; do
        count=$((count + 1))
        if test "$count" -gt 300; then
           echo " server not ready after 5 mins, giving up."
           exit 1
        fi
        printf '.'
        sleep 1
    done
    echo " up."
}

IFS=',' read -ra SERVERS <<< "${SCHEMA_REGISTRY_KAFKASTORE_BOOTSTRAP_SERVERS}"
for server in "${SERVERS[@]}"; do
    server="${server// /}"
    test -z "$server" && continue
    hostport="${server#*://}"   # strip PLAINTEXT:// etc.
    hostport="${hostport%%/*}"
    wait_server "${hostport%:*}" "${hostport##*:}"
done

exec "$@"
