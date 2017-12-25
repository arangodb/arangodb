# Datacenter to datacenter replication deployment 

This Section describes how to deploy all the components needed for datacenter to
datacenter replication.

## ArangoDB cluster 

There are several ways to start an ArangoDB cluster. In this tutorial we will focus on our recommended
way to start ArangoDB: the ArangoDB _Starter_.

Datacenter to datacenter replication requires the `rocksdb` storage engine. In this tutorial the
example setup will have `rocksdb` enabled. If you choose to deploy with a different strategy keep
in mind to set the storage engine.

For the other possibilities to deploy an ArangoDB cluster please refer to the [this](Cluster/README.md) section.

The _Starter_ simplifies things for the operator and will coordinate a distributed cluster startup
across several machines and assign cluster roles automatically.

When started on several machines and enough machines have joined, the starters will start agents,
coordinators and dbservers on these machines.

When running the _Starter_ will supervise its child tasks (namely coordinators, dbservers and agents)
and restart them in case of failure.

To start the cluster using a systemd unit file use the following:

```
[Unit]
Description=Run the ArangoDB Starter 
After=network.target

[Service]
Restart=on-failure
EnvironmentFile=/etc/arangodb.env
EnvironmentFile=/etc/arangodb.env.local
Environment=DATADIR=/var/lib/arangodb/cluster
ExecStartPre=/usr/bin/sh -c "mkdir -p ${DATADIR}"
ExecStart=/usr/bin/arangodb \
    --starter.address=${PRIVATEIP} \
    --starter.data-dir=${DATADIR} \
    --starter.join=${STARTERENDPOINTS} \
    --server.storage-engine=rocksdb \
    --auth.jwt-secret=${CLUSTERSECRETPATH}
TimeoutStopSec=60

[Install]
WantedBy=multi-user.target
```

Note that we set `rocksdb` in the unit service file.

### Cluster authentication

The communication between the cluster nodes use a token (JWT) to authenticate. This must
be shared between cluster nodes.

Sharing secrets is obviously a very delicate topic. The above workflow assumes that the operator
will put a secret in a file named `${CLUSTERSECRETPATH}`.

We recommend to use a dedicated system for managing secrets like HashiCorps' `Vault` or the
secret management of `DC/OS`.

### Required ports

As soon as enough machines have joined, the _Starter_ will begin starting agents, coordinators
and dbservers.

Each of these tasks needs a port to communicate. Please make sure that the following ports
are available on all machines:

- `8529` for coordinators
- `8530` for dbservers
- `8531` for agents

The _Starter_ itself will use port `8528`.

## Kafka & Zookeeper 

- How to deploy zookeeper 
- How to deploy kafka 
- Accessible ports

## Sync Master 

The Sync Master is responsible for managing all synchronization, creating tasks and assigning 
those to workers.
<br/> At least 2 instances muts be deployed in each datacenter.
One instance will be the "leader", the other will be an inactive slave. When the leader 
is gone for a short while, one of the other instances will take over.

With clusters of a significant size, the sync master will require a significant set of resources.
Therefore it is recommended to deploy sync masters on their own servers, equiped with sufficient
CPU power and memory capacity.

To start a sync master using a `systemd` service, use a unit like this:

```
[Unit]
Description=Run ArangoSync in master mode 
After=network.target

[Service]
Restart=on-failure
EnvironmentFile=/etc/arangodb.env 
EnvironmentFile=/etc/arangodb.env.local
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

The sync master needs a TLS server certificate and a 
If you want the service to create a TLS certificate & client authentication 
certificate, for authenticating with sync masters in another datacenter, for every start, 
add this to the `Service` section.

```
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

The sync master must be reachable on a TCP port `${MASTERPORT}` (used with `--server.port` option).
This port must be reachable from inside the datacenter (by sync workers and operations) 
and from inside of the other datacenter (by sync masters in the other datacenter).

## Sync Workers 

The Sync Worker is responsible for executing synchronization tasks.
<br/> For optimal performance at least 1 worker instance must be placed on 
every machine that has an ArangoDB `dbserver` running. This ensures that tasks 
can be executed with minimal network traffic outside of the machine.

Since sync workers will automatically stop once their TLS server certificate expires 
(which is set to 2 years by default),
it is recommended to run at least 2 instances of a worker on every machine in the datacenter.
That way, tasks can still be assigned in the most optimal way, even when a worker in temporarily 
down for a restart.

To start a sync worker using a `systemd` service, use a unit like this:

```
[Unit]
Description=Run ArangoSync in worker mode 
After=network.target

[Service]
Restart=on-failure
EnvironmentFile=/etc/arangodb.env 
EnvironmentFile=/etc/arangodb.env.local
Environment=PORT=8729
ExecStart=/usr/sbin/arangosync run worker \
    --log.level=debug \
    --server.port=${PORT} \
    --server.endpoint=https://${PRIVATEIP}:${PORT} \
    --master.endpoint=${MASTERENDPOINTS} \
    --master.jwtSecret=${MASTERSECRET}
TimeoutStopSec=60

[Install]
WantedBy=multi-user.target
```

The sync worker must be reachable on a TCP port `${PORT}` (used with `--server.port` option).
This port must be reachable from inside the datacenter (by sync masters).

## Prometheus & Grafana (optional)

ArangoSync provides metrics in a format supported by [Prometheus](https://prometheus.io).
We also provide a standard set of dashboards for viewing those metrics in [Grafana](https://grafana.org).

If you want to use these tools, go to their websites for instructions on how to deploy them.

After deployment, you must configure prometheus using a configuration file that instructs 
it about which targets to scrape. For ArangoSync you should configure scrape targets for 
all sync masters and all sync workers. To do so, you can use a configuration such as this:

```
global:
  scrape_interval:     10s # scrape targets every 10 seconds.

scrape_configs:
  # Scrap sync masters
  - job_name: 'sync_master'
    scheme: 'https'
    bearer_token: "${MONITORINGTOKEN}"
    tls_config:
      insecure_skip_verify: true
    static_configs:
      - targets:
        - "${IPMASTERA1}:8629"
        - "${IPMASTERA2}:8629"
        - "${IPMASTERB1}:8629"
        - "${IPMASTERB2}:8629"
        labels:
          type: "master"
    relabel_configs:
      - source_labels: [__address__]
        regex:         ${IPMASTERA1}\:8629|${IPMASTERA2}\:8629
        target_label:  dc
        replacement:   A
      - source_labels: [__address__]
        regex:         ${IPMASTERB1}\:8629|${IPMASTERB2}\:8629
        target_label:  dc
        replacement:   B
      - source_labels: [__address__]
        regex:         ${IPMASTERA1}\:8629|${IPMASTERB1}\:8629
        target_label:  instance
        replacement:   1
      - source_labels: [__address__]
        regex:         ${IPMASTERA2}\:8629|${IPMASTERB2}\:8629
        target_label:  instance
        replacement:   2

  # Scrap sync workers
  - job_name: 'sync_worker'
    scheme: 'https'
    bearer_token: "${MONITORINGTOKEN}"
    tls_config:
      insecure_skip_verify: true
    static_configs:
      - targets: 
        - "${IPWORKERA1}:8729"
        - "${IPWORKERA2}:8729"
        - "${IPWORKERB1}:8729"
        - "${IPWORKERB2}:8729"
        labels:
          type: "worker"
    relabel_configs:
      - source_labels: [__address__]
        regex:         ${IPWORKERA1}\:8729|${IPWORKERA2}\:8729
        target_label:  dc
        replacement:   A
      - source_labels: [__address__]
        regex:         ${IPWORKERB1}\:8729|${IPWORKERB2}\:8729
        target_label:  dc
        replacement:   B
      - source_labels: [__address__]
        regex:         ${IPWORKERA1}\:8729|${IPWORKERB1}\:8729
        target_label:  instance
        replacement:   1
      - source_labels: [__address__]
        regex:         ${IPWORKERA2}\:8729|${IPWORKERB2}\:8729
        target_label:  instance
        replacement:   2
```

Note: The above example assumes 2 datacenters, with 2 sync masters & 2 sync workers 
per datacenter. You have to replace all `${...}` variables in the above configuration 
with applicable values from your environment.

