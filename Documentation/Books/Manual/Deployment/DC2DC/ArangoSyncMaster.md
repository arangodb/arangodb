# ArangoSync Master 

The _ArangoSync Master_ is responsible for managing all synchronization, creating
tasks and assigning those to the _ArangoSync Workers_.
<br/> At least 2 instances muts be deployed in each datacenter.
One instance will be the "leader", the other will be an inactive slave. When the
leader is gone for a short while, one of the other instances will take over.

With clusters of a significant size, the _sync master_ will require a 
significant set of resources. Therefore it is recommended to deploy the _sync masters_
on their own servers, equiped with sufficient CPU power and memory capacity.

To start an _ArangoSync Master_ using a `systemd` service, use a unit like this:

```text
[Unit]
Description=Run ArangoSync in master mode 
After=network.target

[Service]
Restart=on-failure
EnvironmentFile=/etc/arangodb.env 
EnvironmentFile=/etc/arangodb.env.local
LimitNOFILE=8192
ExecStart=/usr/sbin/arangosync run master \
    --log.level=debug \
    --cluster.endpoint=${CLUSTERENDPOINTS} \
    --cluster.jwtSecret=${CLUSTERSECRET} \
    --server.keyfile=${CERTIFICATEDIR}/tls.keyfile \
    --server.client-cafile=${CERTIFICATEDIR}/client-auth-ca.crt \
    --server.endpoint=https://${PUBLICIP}:${MASTERPORT} \
    --server.port=${MASTERPORT} \
    --master.jwtSecret=${MASTERSECRET} \
    --mq.type=kafka \
    --mq.kafka-addr=${KAFKAENDPOINTS} \
    --mq.kafka-client-keyfile=${CERTIFICATEDIR}/kafka-client.key \
    --mq.kafka-cacert=${CERTIFICATEDIR}/tls-ca.crt \
TimeoutStopSec=60

[Install]
WantedBy=multi-user.target
```

The _sync master_ needs a TLS server certificate and a 
If you want the service to create a TLS certificate & client authentication 
certificate, for authenticating with _ArangoSync Masters_ in another datacenter,
for every start, add this to the `Service` section.

```text
ExecStartPre=/usr/bin/sh -c "mkdir -p ${CERTIFICATEDIR}"
ExecStartPre=/usr/sbin/arangosync create tls keyfile \
    --cacert=${CERTIFICATEDIR}/tls-ca.crt \
    --cakey=${CERTIFICATEDIR}/tls-ca.key \
    --keyfile=${CERTIFICATEDIR}/tls.keyfile \
    --host=${PUBLICIP} \
    --host=${PRIVATEIP} \
    --host=${HOST}
ExecStartPre=/usr/sbin/arangosync create client-auth keyfile \
    --cacert=${CERTIFICATEDIR}/tls-ca.crt \
    --cakey=${CERTIFICATEDIR}/tls-ca.key \
    --keyfile=${CERTIFICATEDIR}/kafka-client.key \
    --host=${PUBLICIP} \
    --host=${PRIVATEIP} \
    --host=${HOST}
```

The _ArangoSync Master_ must be reachable on a TCP port `${MASTERPORT}` (used with `--server.port` option).
This port must be reachable from inside the datacenter (by sync workers and operations) 
and from inside of the other datacenter (by sync masters in the other datacenter).

## Recommended deployment environment

Since the _sync masters_ can be CPU intensive when running lots of databases & collections,
it is recommended to run them on dedicated machines with a lot of CPU power.

Consider these machines "pets".