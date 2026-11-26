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

BROKER_COUNT=0
IFS=',' read -ra SERVERS <<< "${SCHEMA_REGISTRY_KAFKASTORE_BOOTSTRAP_SERVERS}"
for server in "${SERVERS[@]}"; do
    server="${server// /}"
    test -z "$server" && continue
    hostport="${server#*://}"   # strip PLAINTEXT:// etc.
    hostport="${hostport%%/*}"
    wait_server "${hostport%:*}" "${hostport##*:}"
    BROKER_COUNT=$((BROKER_COUNT + 1))
done

# Open ports don't mean the brokers have joined the cluster. If the schema
# registry creates its topic too early, its first write can hit a broker that
# doesn't host the new partition yet, and since its producer runs with
# retries=0 it aborts with UnknownTopicOrPartitionException.
# So wait until all brokers are registered and create the topic ourselves.
# (cub runs without COMPONENT so it uses the plain console log4j config instead
# of the not yet templated schema registry one, which logs to an unwritable file)
echo "waiting for ${BROKER_COUNT} brokers to join the cluster"
env -u COMPONENT cub kafka-ready -b "${SCHEMA_REGISTRY_KAFKASTORE_BOOTSTRAP_SERVERS}" "${BROKER_COUNT}" 300

TOPIC="${SCHEMA_REGISTRY_KAFKASTORE_TOPIC:-_schemas}"
REPLICATION_FACTOR="${SCHEMA_REGISTRY_KAFKASTORE_TOPIC_REPLICATION_FACTOR:-3}"
if test "${REPLICATION_FACTOR}" -gt "${BROKER_COUNT}"; then
    REPLICATION_FACTOR="${BROKER_COUNT}"
fi
WORKDIR=$(mktemp -d)
cat > "${WORKDIR}/client.properties" <<EOF
bootstrap.servers=${SCHEMA_REGISTRY_KAFKASTORE_BOOTSTRAP_SERVERS}
EOF
# the settings the schema registry validates its topic against
cat > "${WORKDIR}/topic.yaml" <<EOF
topics:
  - name: ${TOPIC}
    partitions: 1
    replicationFactor: ${REPLICATION_FACTOR}
    config:
      cleanup.policy: compact
EOF
echo "ensuring topic ${TOPIC} exists"
env -u COMPONENT cub ensure-topic --create_if_not_exists "${WORKDIR}/client.properties" "${WORKDIR}/topic.yaml" 300
rm -rf "${WORKDIR}"

exec "$@"
