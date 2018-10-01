<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Troubleshooting datacenter to datacenter replication

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

The _datacenter to datacenter replication_ is a distributed system with a lot
different components. As with any such system, it requires some, but not a lot,
of operational support.

This section includes information on how to troubleshoot the
_datacenter to datacenter replication_.

For a general introduction to the _datacenter to datacenter replication_, please
refer to the [Datacenter to datacenter replication](../../Architecture/DeploymentModes/DC2DC/README.md)
chapter.

## What means are available to monitor status

All of the components of _ArangoSync_ provide means to monitor their status.
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

## What to look for while monitoring status

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

## What to do when problems remain

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

## What to do when a source datacenter is down

When you use ArangoSync for backup of your cluster from one datacenter
to another and the source datacenter has a complete outage, you may consider
switching your applications to the target (backup) datacenter.

This is what you must do in that case:

1. [Stop synchronization](../../Administration/DC2DC/README.md#stopping-synchronization) using:

   ```bash
   arangosync stop sync ...
   ```
   When the source datacenter is completely unresponsive this will not succeed.
   In that case use:

   ```bash
   arangosync abort sync ...
   ```

   See [Stopping synchronization](../../Administration/DC2DC/README.md#stopping-synchronization)
   for how to cleanup the source datacenter when it becomes available again.

2. Verify that configuration has completely stopped using:
   ```bash
   arangosync get status ... -v
   ```

3. Reconfigure your applications to use the target (backup) datacenter.

When the original source datacenter is restored, you may switch roles and
make it the target datacenter. To do so, use `arangosync configure sync ...`
as described in [Reversing synchronization direction](../../Administration/DC2DC/README.md#reversing-synchronization-direction).

## What to do in case of a planned network outage

All ArangoSync tasks send out heartbeat messages out to the other datacenter
to indicate "it is still alive". The other datacenter assumes the connection is
"out of sync" when it does not receive any messages for a certain period of time.

If you're planning some sort of maintenance where you know the connectivity
will be lost for some time (e.g. 3 hours), you can prepare ArangoSync for that
such that it will hold off re-synchronization for a given period of time.

To do so, on both datacenters, run:

```bash
arangosync set message timeout \
    --master.endpoint=<endpoints of sync masters in the datacenter> \
    --auth.user=<username used for authentication of this command> \
    --auth.password=<password of auth.user> \
    3h
```

The last argument is the period that ArangoSync should hold-off resynchronization for.
This can be minutes (e.g. `15m`) or hours (e.g. `3h`).

If maintenance is taking longer than expected, you can use the same command the extend
the hold-off period (e.g. to `4h`).

After the maintenance, use the same command restore the hold-off period to its
default of `1h`.

## What to do in case of a document that exceeds the message queue limits

If you insert/update a document in a collection and the size of that document
is larger than the maximum message size of your message queue, the collection
will no longer be able to synchronize. It will go into a `failed` state.

To recover from that, first remove the document from the ArangoDB cluster
in the source datacenter. After that, for each failed shard, run:

```bash
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
