# Datacenter to datacenter replication.

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

## About 

At some point in the grows of a database, there comes a need for 
replicating it across multiple datacenters.

Reasons for that can be:
- Fallback in case of a disaster in one datacenter.
- Regional availability 
- Separation of concerns 

And many more.

This tutorial describes what the ArangoSync datacenter to datacenter 
replication solution (ArangoSync from now on) offers,
when to use it, when not to use it and  how to configure,
operate, troubleshoot it & keep it safe.

### What is it

ArangoSync is a solution that enables you to asynchronously replicate 
the entire structure and content in an ArangoDB cluster in one place to a cluster 
in another place. Typically it is used from one datacenter to another.
<br/>It is not a solution for replicating single server instances.

The replication done by ArangoSync in **asynchronous**. That means that when 
a client is writing data into the source datacenter, it will consider the 
request finished before the data has been replicated to the other datacenter.
The time needed to completely replicate changes to the other datacenter is
typically in the order of seconds, but this can vary significantly depending on 
load, network & computer capacity.

ArangoSync performs replication in a **single direction** only. That means that 
you can replicate data from cluster A to cluster B or from cluster B to cluster A,
but never at the same time. 
<br/>Data modified in the destination cluster **will be lost!**

Replication is a completely **autonomous** process. Once it is configured it is 
designed to run 24/7 without frequent manual intervention.
<br/>This does not mean that it requires no maintenance or attention at all.
<br/>As with any distributed system some attention is needed to monitor its operation 
and keep it secure (e.g. certificate & password rotation).

Once configured, ArangoSync will replicate both **structure and data** of an 
**entire cluster**. This means that there is no need to make additional configuration 
changes when adding/removing databases or collections.
<br/>Also meta data such as users, foxx application & jobs are automatically replicated.

### When to use it... and when not

ArangoSync is a good solution in all cases where you want to replicate 
data from one cluster to another without the requirement that the data 
is available immediately in the other cluster.

ArangoSync is not a good solution when one of the following applies:
- You want to replicate data from cluster A to cluster B and from cluster B
  to cluster A at the same time. 
- You need synchronous replication between 2 clusters.
- There is no network connection betwee cluster A and B.
- You want complete control over which database, collection & documents are replicate and which not.

## Requirements 

To use ArangoSync you need the following:

- Two datacenters, each running an ArangoDB Enterprise cluster, version 3.3 or higher.
- A network connection between both datacenters with accessible endpoints
  for several components (see individual components for details).
- TLS certificates for ArangoSync master instances (can be self-signed).
- TLS certificates for Kafka brokers (can be self-signed).
- Optional (but recommended) TLS certificates for ArangoDB clusters (can be self-signed).
- Client certificates CA for ArangoSync masters (typically self-signed).
- Client certificates for ArangoSync masters (typically self-signed).
- At least 2 instances of the ArangoSync master in each datacenter.
- One instances of the ArangoSync worker on every machine in each datacenter.

Note: In several places you will need a (x509) certificate. 
<br/>The [certificates](#certificates) section below provides more guidance for creating 
and renewing these certificates.

Besides the above list, you probably want to use the following:

- An orchestrator to keep all components running. In this tutorial we will use `systemd` as an example.
- A log file collector for centralized collection & access to the logs of all components.
- A metrics collector & viewing solution such as Prometheus + Grafana.

## Deployment 

In the following paragraphs you'll learn how to deploy all the components 
needed for datacenter to datacenter replication.

### ArangoDB cluster 

There are several ways to start an ArangoDB cluster. In this tutorial we will focus on our recommended
way to start ArangoDB: the ArangoDB starter.

Datacenter to datacenter replication requires the `rocksdb` storage engine. In this tutorial the
example setup will have `rocksdb` enabled. If you choose to deploy with a different strategy keep
in mind to set the storage engine.

Also see other possibilities to [deploy an ArangoDB cluster](Cluster.md).

The starter simplifies things for the operator and will coordinate a distributed cluster startup
across several machines and assign cluster roles automatically.

When started on several machines and enough machines have joined, the starters will start agents,
coordinators and dbservers on these machines.

When running the starter will supervise its child tasks (namely coordinators, dbservers and agents)
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

#### Cluster authentication

The communication between the cluster nodes use a token (JWT) to authenticate. This must
be shared between cluster nodes.

Sharing secrets is obviously a very delicate topic. The above workflow assumes that the operator
will put a secret in a file named `${CLUSTERSECRETPATH}`.

We recommend to use a dedicated system for managing secrets like HashiCorps' `Vault` or the
secret management of `DC/OS`.

#### Required ports

As soon as enough machines have joined, the starter will begin starting agents, coordinators
and dbservers.

Each of these tasks needs a port to communicate. Please make sure that the following ports
are available on all machines:

- `8529` for coordinators
- `8530` for dbservers
- `8531` for agents

The starter itself will use port `8528`.

### Kafka & Zookeeper 

- How to deploy zookeeper 
- How to deploy kafka 
- Accessible ports

### Sync Master 

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

### Sync Workers 

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

### Prometheus & Grafana (optional)

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

## Configuration 

Once all components of the ArangoSync solution have been deployed and are 
running properly, ArangoSync will not automatically replicate database structure 
and content. For that, it is is needed to configure synchronization.

To configure synchronization, you need the following:
- The endpoint of the sync master in the target datacenter.
- The endpoint of the sync master in the source datacenter.
- A certificate (in keyfile format) used for client authentication of the sync master 
  (with the sync master in the source datacenter).
- A CA certificate (public key only) for verifying the integrity of the sync masters.
- A username+password pair (or client certificate) for authenticating the configure 
  require with the sync master (in the target datacenter)

With that information, run:
```
arangosync configure sync \
  --master.endpoint=<endpoints of sync masters in target datacenter> \
  --master.keyfile=<keyfile of of sync masters in target datacenter> \
  --source.endpoint=<endpoints of sync masters in source datacenter> \
  --source.cacert=<public key of CA certificate used to verify sync master in source datacenter> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user>
```

The command will finish quickly. Afterwards it will take some time until 
the clusters in both datacenters are in sync.

Use the following command to inspect the status of the synchronization of a datacenter:
```
arangosync get status \
  --master.endpoint=<endpoints of sync masters in datacenter of interest> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user> \
  -v
```
Note: Invoking this command on the target datacenter will return different results from
invoking it on the source datacenter. You need insight in both results to get a "complete picture".

Where the `get status` command gives insight in the status of synchronization, there 
are more detailed commands to give insight in tasks & registered workers.

Use the following command to get a list of all synchronization tasks in a datacenter:
```
arangosync get tasks \
  --master.endpoint=<endpoints of sync masters in datacenter of interest> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user> \
  -v
```

Use the following command to get a list of all masters in a datacenter and know which master is the current leader:
```
arangosync get masters \
  --master.endpoint=<endpoints of sync masters in datacenter of interest> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user> \
  -v
```

Use the following command to get a list of all workers in a datacenter:
```
arangosync get workers \
  --master.endpoint=<endpoints of sync masters in datacenter of interest> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user> \
  -v
```

### Stop synchronization

If you no longer want to synchronize data from a source to a target datacenter 
you must stop it. To do so, run the following command:

```
arangosync stop sync \
  --master.endpoint=<endpoints of sync masters in target datacenter> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user>
```

The command will wait until synchronization has completely stopped before returning.
If the synchronization is not completely stopped within a reasonable period (2 minutes by default)
the command will fail.

If the source datacenter is no longer available it is not possible to stop synchronization in 
a graceful manner. If that happens abort the synchronization with the following command:
```
arangosync abort sync \
  --master.endpoint=<endpoints of sync masters in target datacenter> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user>
```
If the source datacenter recovers after an `abort sync` has been executed, it is 
needed to "cleanup" ArangoSync in the source datacenter. 
To do so, execute the following command:
```
arangosync abort outgoing sync \
  --master.endpoint=<endpoints of sync masters in source datacenter> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user>
```

### Reversing synchronization direction 

If you want to reverse the direction of synchronization (e.g. after a failure
in datacenter A and you switched to the datacenter B for fallback), you 
must first stop (or abort) the original synchronization.

Once that is finished (and cleanup has been applied in case of abort),
you must now configure the synchronization again, but with swapped 
source & target settings.

## Operations & Maintenance 

ArangoSync is a distributed system with a lot different components.
As with any such system, it requires some, but not a lot, of operational 
support. 

### What means are available to monitor status 

All of the components of ArangoSync provide means to monitor their status.
Below you'll find an overview per component.

- Sync master & workers: The `arangosync` servers running as either master
  or worker, provide:
  - A status API, see `arangosync get status`. Make sure that all statuses report `running`.
    <br/>For even more detail the following commands are also available:
    `arangosync get tasks`, `arangosync get masters` & `arangosync get workers`.
  - A log on the standard output. Log levels can be configured using `--log.level` settings.
  - A metrics API `GET /metrics`. This API is compatible with Prometheus.
    Sample Grafana dashboards for inspecting these metrics are available.

- ArangoDB cluster: The `arangod` servers that make up the ArangoDB cluster 
  provide:
  - A log file. This is configurable with settings with a `log.` prefix.
  E.g. `--log.output=file://myLogFile` or `--log.level=info`.
  - A statistics API `GET /_admin/statistics`

- Kafka cluster: The kafka brokers provide:
  - A log file, see settings with `log.` prefix in its `server.properties` configuration file.

- Zookeeper: The zookeeper agents provide:
  - A log on standard output.

### What to look for while monitoring status

The very first thing to do when monitoring the status of ArangoSync is to 
look into the status provided by `arangosync get status ... -v`.
When not everything is in the `running` state (on both datacenters), this is an 
indication that something may be wrong. In case that happens, give it some time 
(incremental synchronization may take quite some time for large collections)
and look at the status again. If the statuses do not change (or change, but not reach `running`)
it is time to inspects the metrics & log files.
<br/> When the metrics or logs seem to indicate a problem in a sync master or worker, it is 
safe to restart it, as long as only 1 instance is restarted at a time.
Give restarted instances some time to "catch up".

### What to do when problems remain 

When a problem remains and restarting masters/workers does not solve the problem,
contact support. Make sure to include provide support with the following information:

- Output of `arangosync get version ...` on both datacenters.
- Output of `arangosync get status ... -v` on both datacenters.
- Output of `arangosync get tasks ... -v` on both datacenters.
- Output of `arangosync get masters ... -v` on both datacenters.
- Output of `arangosync get workers ... -v` on both datacenters.
- Log files of all components
- A complete description of the problem you observed and what you did to resolve it.

- How to monitor status of ArangoSync
- How to keep it alive
- What to do in case of failures or bugs

### What to do when a source datacenter is down

When you use ArangoSync for backup of your cluster from one datacenter
to another and the source datacenter has a complete outage, you may consider 
switching your applications to the target (backup) datacenter.

This is what you must do in that case.

1. [Stop configuration](#stop-synchronization) using `arangosync stop sync ...`.
   <br/>When the source datacenter is completely unresponsive this will not 
   succeed. In that case use `arangosync abort sync ...`. 
   <br/>See [Configuration](#stop-synchronization) for how to cleanup the source datacenter when 
   it becomes available again.
2. Verify that configuration has completely stopped using `arangosync get status ... -v`.
3. Reconfigure your applications to use the target (backup) datacenter.

When the original source datacenter is restored, you may switch roles and 
make it the target datacenter. To do so, use `arangosync configure sync ...` 
as described in [Configuration](#configuration).

### What to do in case of a planned network outage.

All ArangoSync tasks send out heartbeat messages out to the other datacenter 
to indicate "it is still alive". The other datacenter assumes the connection is 
"out of sync" when it does not receive any messages for a certain period of time.

If you're planning some sort of maintenance where you know the connectivity 
will be lost for some time (e.g. 3 hours), you can prepare ArangoSync for that 
such that it will hold of re-synchronization for a given period of time.

To do so, on both datacenters, run:
```
arangosync set message timeout \
    --master.endpoint=<endpoints of sync masters in the datacenter> \
    --auth.user=<username used for authentication of this command> \
    --auth.password=<password of auth.user> \
    3h
```
The last argument is the period that ArangoSync should hold-of resynchronization for.
This can be minutes (e.g. `15m`) or hours (e.g. `3h`).

If maintenance is taking longer than expected, you can use the same command the extend
the hold of period (e.g. to `4h`).

After the maintenance, use the same command restore the hold of period to its default of `1h`.

### What to do in case of a document that exceeds the message queue limits.

If you insert/update a document in a collection and the size of that document
is larger than the maximum message size of your message queue, the collection
will no longer be able to synchronize. It will go into a `failed` state.

To recover from that, first remove the document from the ArangoDB cluster
in the source datacenter. After that, for each failed shard, run:
```
arangosync reset failed shard \
    --master.endpoint=<endpoints of sync masters in the datacenter> \
    --auth.user=<username used for authentication of this command> \
    --auth.password=<password of auth.user> \
    --database=<name of the database> \
    --collection=<name of the collection> \
    --shard=<index of the shard (starting at 0)>
```

After this command, a new set of tasks will be started to synchronize the shard.
It can take some time for the shard to reach `running` state.

### Metrics

ArangoSync (master & worker) provide metrics that can be used for monitoring the ArangoSync 
solution. These metrics are available using the following HTTPS endpoints:

- GET `/metrics`: Provides metrics in a format supported by Prometheus.
- GET `/metrics.json`: Provides the same metrics in JSON format.

Both endpoints include help information per metrics.

Note: Both endpoints require authentication. Besides the usual authentication methods 
these endpoints are also accessible using a special bearer token specified using the `--monitoring.token`
command line option.

The Prometheus output (`/metrics`) looks like this:
```
...
# HELP arangosync_master_worker_registrations Total number of registrations
# TYPE arangosync_master_worker_registrations counter
arangosync_master_worker_registrations 2
# HELP arangosync_master_worker_storage Number of times worker info is stored, loaded
# TYPE arangosync_master_worker_storage counter
arangosync_master_worker_storage{kind="",op="save",result="success"} 20
arangosync_master_worker_storage{kind="empty",op="load",result="success"} 1
...
```

The JSON output (`/metrics.json`) looks like this:
```
{
  ...
  "arangosync_master_worker_registrations": {
    "help": "Total number of registrations",
    "type": "counter",
    "samples": [
      {
        "value": 2
      }
    ]
  },
  "arangosync_master_worker_storage": {
    "help": "Number of times worker info is stored, loaded",
    "type": "counter",
    "samples": [
      {
        "value": 8,
        "labels": {
          "kind": "",
          "op": "save",
          "result": "success"
        }
      },
      {
        "value": 1,
        "labels": {
          "kind": "empty",
          "op": "load",
          "result": "success"
        }
      }
    ]
  }
  ...
}
```

Hint: To get a list of a metrics and their help information, run:
```
alias jq='docker run --rm -i realguess/jq jq'
curl -sk -u "<user>:<password>" https://<syncmaster-IP>:8629/metrics.json | \
  jq 'with_entries({key: .key, value:.value.help})'
```

## Security

### Firewall settings

The components of ArangoSync use (TCP) network connections to communicate with each other.
Below you'll find an overview of these connections and the TCP ports that should be accessible.

1.  The sync masters must be allowed to connect to the following components
    within the same datacenter:

    - ArangoDB agents and coordinators (default ports: `8531` and `8529`)
    - Kafka brokers (default port `9092`)
    - Sync workers (default port `8729`)

    Additionally the sync masters must be allowed to connect to the sync masters in the other datacenter.

    By default the sync masters will operate on port `8629`.

2.  The sync workers must be allowed to connect to the following components within the same datacenter:

    - ArangoDB coordinators (default port `8529`)
    - Kafka brokers (default port `9092`)
    - Sync masters (default port `8629`)

    By default the sync workers will operate on port `8729`.

    Additionally the sync workers must be allowed to connect to the Kafka brokers in the other datacenter.

3.  Kafka

    The kafka brokers must be allowed to connect to the following components within the same datacenter:

    - Other kafka brokers (default port `9092`)
    - Zookeeper (default ports `2181`, `2888` and `3888`)

    The default port for kafka is `9092`. The default kafka installation will also expose some prometheus
    metrics on port `7071`. To gain more insight into kafka open this port for your prometheus
    installation.

4.  Zookeeper

    The zookeeper agents must be allowed to connect to the following components within the same datacenter:

    - Other zookeeper agents

    The setup here is a bit special as zookeeper uses 3 ports for different operations. All agents need to
    be able to connect to all of these ports.

    By default Zookeeper uses:
    
    - port `2181` for client communication
    - port `2888` for follower communication
    - port `3888` for leader elections

### Certificates 

Digital certificates are used in many places in ArangoSync for both encryption 
and authentication.

<br/> In ArangoSync all network connections are using Transport Layer Security (TLS),
a set of protocols that ensure that all network traffic is encrypted.
For this TLS certificates are used. The server side of the network connection 
offers a TLS certificate. This certificate is (often) verified by the client side of the network
connection, to ensure that the certificate is signed by a trusted Certificate Authority (CA). 
This ensures the integrity of the server.
<br/> In several places additional certificates are used for authentication. In those cases 
the client side of the connection offers a client certificate (on top of an existing TLS connection).
The server side of the connection uses the client certificate to authenticate 
the client and (optionally) decides which rights should be assigned to the client.

Note: ArangoSync does allow the use of certificates signed by a well know CA (eg. verisign)
however it is more convenient (and common) to use your own CA.

#### Formats

All certificates are x509 certificates with a public key, a private key and 
an optional chain of certificates used to sign the certificate (this chain is 
typically provided by the Certificate Authority (CA)).
<br/>Depending on their use, certificates stored in a different format.

The following formats are used:

- Public key only (`.crt`): A file that contains only the public key of 
  a certificate with an optional chain of parent certificates (public keys of certificates
  used to signed the certificate).
  <br/>Since this format contains only public keys, it is not a problem if its contents 
  are exposed. It must still be store it in a safe place to avoid losing it.
- Private key only (`.key`): A file that contains only the private key of a certificate.
  <br/>It is vital to protect these files and store them in a safe place.
- Keyfile with public & private key (`.keyfile`): A file that contains the public key of 
  a certificate, an optional chain of parent certificates and a private key.
  <br/>Since this format also contains a private key, it is vital to protect these files 
  and store them in a safe place.
- Java keystore (`.jks`): A file containing a set of public and private keys.
  <br/>It is possible to protect access to the content of this file using a keystore password.
  <br/>Since this format can contain private keys, it is vital to protect these files 
  and store them in a safe place (even when its content is protected with a keystore password).

#### Creating certificates

ArangoSync provides commands to create all certificates needed.

##### TLS server certificates

To create a certificate used for TLS servers in the **keyfile** format,
you need the public key of the CA (`--cacert`), the private key of 
the CA (`--cakey`) and one or more hostnames (or IP addresses).
Then run:
```
arangosync create tls keyfile \
    --cacert=my-tls-ca.crt --cakey=my-tls-ca.key \
    --host=<hostname> \
    --keyfile=my-tls-cert.keyfile
```
Make sure to store the generated keyfile (`my-tls-cert.keyfile`) in a safe place.

To create a certificate used for TLS servers in the **crt** & **key** format,
you need the public key of the CA (`--cacert`), the private key of 
the CA (`--cakey`) and one or more hostnames (or IP addresses).
Then run:
```
arangosync create tls certificate \
    --cacert=my-tls-ca.crt --cakey=my-tls-ca.key \
    --host=<hostname> \
    --cert=my-tls-cert.crt \
    --key=my-tls-cert.key \
```
Make sure to protect and store the generated files (`my-tls-cert.crt` & `my-tls-cert.key`) in a safe place.

##### Client authentication certificates

To create a certificate used for client authentication in the **keyfile** format,
you need the public key of the CA (`--cacert`), the private key of 
the CA (`--cakey`) and one or more hostnames (or IP addresses) or email addresses.
Then run:
```
arangosync create client-auth keyfile \
    --cacert=my-client-auth-ca.crt --cakey=my-client-auth-ca.key \
    [--host=<hostname> | --email=<emailaddress>] \
    --keyfile=my-client-auth-cert.keyfile 
```
Make sure to protect and store the generated keyfile (`my-client-auth-cert.keyfile`) in a safe place.

##### CA certificates

To create a CA certificate used to **sign TLS certificates**, run:
```
arangosync create tls ca \
    --cert=my-tls-ca.crt --key=my-tls-ca.key 
```
Make sure to protect and store both generated files (`my-tls-ca.crt` & `my-tls-ca.key`) in a safe place.
<br/>Note: CA certificates have a much longer lifetime than normal certificates. 
Therefore even more care is needed to store them safely.

To create a CA certificate used to **sign client authentication certificates**, run:
```
arangosync create client-auth ca \
    --cert=my-client-auth-ca.crt --key=my-client-auth-ca.key 
```
Make sure to protect and store both generated files (`my-client-auth-ca.crt` & `my-client-auth-ca.key`) 
in a safe place.
<br/>Note: CA certificates have a much longer lifetime than normal certificates. 
Therefore even more care is needed to store them safely.

#### Renewing certificates

All certificates have meta information in them the limit their use in function,
target & lifetime.
<br/> A certificate created for client authentication (function) cannot be used as a TLS server certificate
(same is true for the reverse).
<br/> A certificate for host `myserver` (target) cannot be used for host `anotherserver`.
<br/> A certficiate that is valid until October 2017 (limetime) cannot be used after October 2017.

If anything changes in function, target or lifetime you need a new certificate.

The procedure for creating a renewed certificate is the same as for creating a "first" certificate.
<br/> After creating the renewed certificate the process(es) using them have to be updated.
This mean restarting them. All ArangoSync components are designed to support stopping and starting 
single instances, but do not restart more than 1 instance at the same time. 
As soon as 1 instance has been restarted, give it some time to "catch up" before restarting 
the next instance.
