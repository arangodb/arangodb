<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Datacenter to datacenter replication administration

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

This Section includes information related to the administration of the _datacenter
to datacenter replication_.

For a general introduction to the _datacenter to datacenter replication_, please
refer to the [Datacenter to datacenter replication](../../Architecture/DeploymentModes/DC2DC/README.md)
chapter.

## Starting synchronization

Once all components of the _ArangoSync_ solution have been deployed and are
running properly, _ArangoSync_ will not automatically replicate database structure
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

## Inspect status

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

Where the `get status` command gives insight in the status of synchronization, there
are more detailed commands to give insight in tasks & registered workers.

Use the following command to get a list of all synchronization tasks in a datacenter:

```bash
arangosync get tasks \
  --master.endpoint=<endpoints of sync masters in datacenter of interest> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user> \
  -v
```

Use the following command to get a list of all masters in a datacenter and know which master is the current leader:

```bash
arangosync get masters \
  --master.endpoint=<endpoints of sync masters in datacenter of interest> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user> \
  -v
```

Use the following command to get a list of all workers in a datacenter:

```bash
arangosync get workers \
  --master.endpoint=<endpoints of sync masters in datacenter of interest> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user> \
  -v
```

## Stopping synchronization

If you no longer want to synchronize data from a source to a target datacenter
you must stop it. To do so, run the following command:

```bash
arangosync stop sync \
  --master.endpoint=<endpoints of sync masters in target datacenter> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user>
```

The command will first ensure that all shards in the receiving cluster are
completely in-sync with the shards in the sending cluster.
In order to achieve that, the sending cluster will be switched to read/only mode.
After the synchronization has stopped, the sending cluster will be switched
back to read/write mode.

The command will then wait until synchronization has completely stopped before returning.
If the synchronization is not completely stopped within a reasonable period (2 minutes by default)
the command will fail.

If you do not want to wait for all shards in the receiving cluster to be
completely in-sync with the shards in the sending cluster, add an `--ensure-in-sync=false`
argument to the `stop sync` command.

If the source datacenter is no longer available it is not possible to stop synchronization in
a graceful manner. If that happens abort the synchronization with the following command:

```bash
arangosync abort sync \
  --master.endpoint=<endpoints of sync masters in target datacenter> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user>
```

If the source datacenter recovers after an `abort sync` has been executed, it is
needed to "cleanup" ArangoSync in the source datacenter.
To do so, execute the following command:

```bash
arangosync abort outgoing sync \
  --master.endpoint=<endpoints of sync masters in source datacenter> \
  --auth.user=<username used for authentication of this command> \
  --auth.password=<password of auth.user>
```

## Reversing synchronization direction

If you want to reverse the direction of synchronization (e.g. after a failure
in datacenter A and you switched to the datacenter B for fallback), you
must first stop (or abort) the original synchronization.

Once that is finished (and cleanup has been applied in case of abort),
you must now configure the synchronization again, but with swapped
source & target settings.
