<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Datacenter to datacenter Replication

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

## About

At some point in the growth of a database, there comes a need for
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

The replication done by ArangoSync is **asynchronous**. This means that when
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
<br/>Also meta data such as users, Foxx application & jobs are automatically replicated.

### When to use it... and when not

ArangoSync is a good solution in all cases where you want to replicate
data from one cluster to another without the requirement that the data
is available immediately in the other cluster.

ArangoSync is not a good solution when one of the following applies:

- You want to replicate data from cluster A to cluster B and from cluster B
  to cluster A at the same time.
- You need synchronous replication between 2 clusters.
- There is no network connection between cluster A and B.
- You want complete control over which database, collection & documents are replicate and which not.

## Requirements

To use ArangoSync you need the following:

- Two datacenters, each running an ArangoDB Enterprise Edition cluster,
  version 3.3 or higher, using the RocksDB storage engine.
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

In the following paragraphs you'll learn which components have to be deployed
for datacenter to datacenter replication using the `direct` message queue.
For detailed deployment instructions or instructions for the `kafka` message queue,
consult the [reference manual](../../Deployment/DC2DC/README.md).

### ArangoDB cluster

Datacenter to datacenter replication requires an ArangoDB cluster in both data centers,
configured with the `rocksdb` storage engine.

Since the cluster agents are so critical to the availability of both the ArangoDB and the ArangoSync cluster,
it is recommended to run agents on dedicated machines. Consider these machines "pets".

Coordinators and DBServers can be deployed on other machines that should be considered "cattle".

### Sync Master

The Sync Master is responsible for managing all synchronization, creating tasks and assigning
those to workers.
<br/> At least 2 instances must be deployed in each datacenter.
One instance will be the "leader", the other will be an inactive slave. When the leader
is gone for a short while, one of the other instances will take over.

With clusters of a significant size, the sync master will require a significant set of resources.
Therefore it is recommended to deploy sync masters on their own servers, equipped with sufficient
CPU power and memory capacity.

The sync master must be reachable on a TCP port 8629 (default).
This port must be reachable from inside the datacenter (by sync workers and operations)
and from inside of the other datacenter (by sync masters in the other datacenter).

Since the sync masters can be CPU intensive when running lots of databases & collections,
it is recommended to run them on dedicated machines with a lot of CPU power.

Consider these machines "pets".

### Sync Workers

The Sync Worker is responsible for executing synchronization tasks.
<br/> For optimal performance at least 1 worker instance must be placed on
every machine that has an ArangoDB DBServer running. This ensures that tasks
can be executed with minimal network traffic outside of the machine.

Since sync workers will automatically stop once their TLS server certificate expires
(which is set to 2 years by default),
it is recommended to run at least 2 instances of a worker on every machine in the datacenter.
That way, tasks can still be assigned in the most optimal way, even when a worker in temporarily
down for a restart.

The sync worker must be reachable on a TCP port 8729 (default).
This port must be reachable from inside the datacenter (by sync masters).

The sync workers should be run on all machines that also contain an ArangoDB DBServer.
The sync worker can be memory intensive when running lots of databases & collections.

Consider these machines "cattle".

### Prometheus & Grafana (optional)

ArangoSync provides metrics in a format supported by [Prometheus](https://prometheus.io).
We also provide a standard set of dashboards for viewing those metrics in [Grafana](https://grafana.org).

If you want to use these tools, go to their websites for instructions on how to deploy them.

After deployment, you must configure prometheus using a configuration file that instructs
it about which targets to scrape. For ArangoSync you should configure scrape targets for
all sync masters and all sync workers.
Consult the [reference manual](../../Deployment/DC2DC/PrometheusGrafana.md) for a sample configuration.

Prometheus can be a memory & CPU intensive process. It is recommended to keep them
on other machines than used to run the ArangoDB cluster or ArangoSync components.

Consider these machines "cattle", unless you configure alerting on prometheus,
in which case it is recommended to consider these machines "pets".

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

```bash
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

```bash
arangosync get status \
  --master.endpoint=<endpoints of sync masters in datacenter of interest> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user> \
  -v
```

Note: Invoking this command on the target datacenter will return different results from
invoking it on the source datacenter. You need insight in both results to get a "complete picture".

ArangoSync has more command to inspect the status of synchronization.
Consult the [reference manual](../../Administration/DC2DC/README.md#inspect-status) for details.

### Stop synchronization

If you no longer want to synchronize data from a source to a target datacenter
you must stop it. To do so, run the following command:

```bash
arangosync stop sync \
  --master.endpoint=<endpoints of sync masters in target datacenter> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user>
```

The command will wait until synchronization has completely stopped before returning.
If the synchronization is not completely stopped within a reasonable period (2 minutes by default)
the command will fail.

If the source datacenter is no longer available it is not possible to stop synchronization in
a graceful manner. Consult the [reference manual](../../Administration/DC2DC/README.md#stopping-synchronization)
for instructions how to abort synchronization in this case.

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

### 'What if ...'

Please consult the [reference manual](../../Troubleshooting/DC2DC/README.md)
for details descriptions of what to do in case of certain problems and how and
what information to provide to support so they can assist you best when needed.

### Metrics

ArangoSync (master & worker) provide metrics that can be used for monitoring the ArangoSync
solution. These metrics are available using the following HTTPS endpoints:

- GET `/metrics`: Provides metrics in a format supported by Prometheus.
- GET `/metrics.json`: Provides the same metrics in JSON format.

Both endpoints include help information per metrics.

Note: Both endpoints require authentication. Besides the usual authentication methods
these endpoints are also accessible using a special bearer token specified using the `--monitoring.token`
command line option.

Consult the [reference manual](../../Monitoring/DC2DC/README.md#metrics)
for sample output of the metrics endpoints.

## Security

### Firewall settings

The components of ArangoSync use (TCP) network connections to communicate with each other.

Consult the [reference manual](../../Security/DC2DC/README.md#firewall-settings)
for a detailed list of connections and the ports that should be accessible.

### Certificates

Digital certificates are used in many places in ArangoSync for both encryption
and authentication.

In ArangoSync all network connections are using Transport Layer Security (TLS),
a set of protocols that ensure that all network traffic is encrypted.
For this TLS certificates are used. The server side of the network connection
offers a TLS certificate. This certificate is (often) verified by the client side of the network
connection, to ensure that the certificate is signed by a trusted Certificate Authority (CA).
This ensures the integrity of the server.

In several places additional certificates are used for authentication. In those cases
the client side of the connection offers a client certificate (on top of an existing TLS connection).
The server side of the connection uses the client certificate to authenticate
the client and (optionally) decides which rights should be assigned to the client.

Note: ArangoSync does allow the use of certificates signed by a well know CA (eg. verisign)
however it is more convenient (and common) to use your own CA.

Consult the [reference manual](../../Security/DC2DC/README.md#certificates)
for detailed instructions on how to create these certificates.

#### Renewing certificates

All certificates have meta information in them the limit their use in function,
target & lifetime.
<br/> A certificate created for client authentication (function) cannot be used as a TLS
server certificate (same is true for the reverse).
<br/> A certificate for host `myserver` (target) cannot be used for host `anotherserver`.
<br/> A certificate that is valid until October 2017 (lifetime) cannot be used after October 2017.

If anything changes in function, target or lifetime you need a new certificate.

The procedure for creating a renewed certificate is the same as for creating a "first" certificate.
<br/> After creating the renewed certificate the process(es) using them have to be updated.
This mean restarting them. All ArangoSync components are designed to support stopping and starting
single instances, but do not restart more than 1 instance at the same time.
As soon as 1 instance has been restarted, give it some time to "catch up" before restarting
the next instance.
