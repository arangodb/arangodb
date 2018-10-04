<!-- don't edit here, it's from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Troubleshooting

While Kubernetes and the ArangoDB Kubernetes operator will automatically
resolve a lot of issues, there are always cases where human attention
is needed.

This chapter gives your tips & tricks to help you troubleshoot deployments.

## Where to look

In Kubernetes all resources can be inspected using `kubectl` using either
the `get` or `describe` command.

To get all details of the resource (both specification & status),
run the following command:

```bash
kubectl get <resource-type> <resource-name> -n <namespace> -o yaml
```

For example, to get the entire specification and status
of an `ArangoDeployment` resource named `my-arangodb` in the `default` namespace,
run:

```bash
kubectl get ArangoDeployment my-arango -n default -o yaml
# or shorter
kubectl get arango my-arango -o yaml
```

Several types of resources (including all ArangoDB custom resources) support
events. These events show what happened to the resource over time.

To show the events (and most important resource data) of a resource,
run the following command:

```bash
kubectl describe <resource-type> <resource-name> -n <namespace>
```

## Getting logs

Another invaluable source of information is the log of containers being run
in Kubernetes.
These logs are accessible through the `Pods` that group these containers.

To fetch the logs of the default container running in a `Pod`, run:

```bash
kubectl logs <pod-name> -n <namespace>
# or with follow option to keep inspecting logs while they are written
kubectl logs <pod-name> -n <namespace> -f
```

To inspect the logs of a specific container in `Pod`, add `-c <container-name>`.
You can find the names of the containers in the `Pod`, using `kubectl describe pod ...`.

{% hint 'info' %}
Note that the ArangoDB operators are being deployed themselves as a Kubernetes `Deployment`
with 2 replicas. This means that you will have to fetch the logs of 2 `Pods` running
those replicas.
{% endhint %}

## What if

### The `Pods` of a deployment stay in `Pending` state

There are two common causes for this.

1) The `Pods` cannot be scheduled because there are not enough nodes available.
   This is usually only the case with a `spec.environment` setting that has a value of `Production`.

   Solution:
Add more nodes.

1) There are no `PersistentVolumes` available to be bound to the `PersistentVolumeClaims`
   created by the operator.

    Solution:
Use `kubectl get persistentvolumes` to inspect the available `PersistentVolumes`
and if needed, use the [`ArangoLocalStorage` operator](./StorageResource.md) to provision `PersistentVolumes`.

### When restarting a `Node`, the `Pods` scheduled on that node remain in `Terminating` state

When a `Node` no longer makes regular calls to the Kubernetes API server, it is
marked as not available. Depending on specific settings in your `Pods`, Kubernetes
will at some point decide to terminate the `Pod`. As long as the `Node` is not
completely removed from the Kubernetes API server, Kubernetes will try to use
the `Node` itself to terminate the `Pod`.

The `ArangoDeployment` operator recognizes this condition and will try to replace those
`Pods` with `Pods` on different nodes. The exact behavior differs per type of server.

### What happens when a `Node` with local data is broken

When a `Node` with `PersistentVolumes` hosted on that `Node` is broken and
cannot be repaired, the data in those `PersistentVolumes` is lost.

If an `ArangoDeployment` of type `Single` was using one of those `PersistentVolumes`
the database is lost and must be restored from a backup.

If an `ArangoDeployment` of type `ActiveFailover` or `Cluster` was using one of
those `PersistentVolumes`, it depends on the type of server that was using the volume.

- If an `Agent` was using the volume, it can be repaired as long as 2 other agents are still     healthy.
- If a `DBServer` was using the volume, and the replication factor of all database
  collections is 2 or higher, and the remaining dbservers are still healthy,
  the cluster will duplicate the remaining replicas to
  bring the number of replicas back to the original number.
- If a `DBServer` was using the volume, and the replication factor of a database
  collection is 1 and happens to be stored on that dbserver, the data is lost.
- If a single server of an `ActiveFailover` deployment was using the volume, and the
  other single server is still healthy, the other single server will become leader.
  After replacing the failed single server, the new follower will synchronize with
  the leader.
