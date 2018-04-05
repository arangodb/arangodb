<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Storage

An ArangoDB cluster relies heavily on fast persistent storage.
The ArangoDB Kubernetes Operator uses `PersistentVolumeClaims` to deliver
the storage to Pods that need them.

## Storage configuration

In the cluster resource, one can specify the type of storage
used by groups of servers using the `spec.<group>.storageClassName`
setting.

The amount of storage needed is configured using the
`spec.<group>.resources.requests.storage` setting.

Note that configuring storage is done per group of servers.
It is not possible to configure storage per individual
server.

## Local storage

For optimal performance, ArangoDB should be configured with locally attached
SSD storage.

To accomplish this, one must create `PersistentVolumes` for all servers that
need persistent storage (single, agents & dbservers).
E.g. for a `Cluster` with 3 agents and 5 dbservers, you must create 8 volumes.

Note that each volume must have a capacity that is equal to or higher than the
capacity needed for each server.

To select the correct node, add a required node-affinity annotation as shown
in the example below.

```yaml
apiVersion: v1
kind: PersistentVolume
metadata:
  name: volume-agent-1
  annotations:
        "volume.alpha.kubernetes.io/node-affinity": '{
            "requiredDuringSchedulingIgnoredDuringExecution": {
                "nodeSelectorTerms": [
                    { "matchExpressions": [
                        { "key": "kubernetes.io/hostname",
                          "operator": "In",
                          "values": ["node-1"]
                        }
                    ]}
                 ]}
              }'
spec:
    capacity:
      storage: 100Gi
    accessModes:
    - ReadWriteOnce
    persistentVolumeReclaimPolicy: Delete
    storageClassName: local-ssd
    local:
      path: /mnt/disks/ssd1
```

For Kubernetes 1.9 and up, you should create a `StorageClass` which is configured
to bind volumes on their first use as shown in the example below.
This ensures that the Kubernetes scheduler takes all constraints on a `Pod`
that into consideration before binding the volume to a claim.

```yaml
kind: StorageClass
apiVersion: storage.k8s.io/v1
metadata:
  name: local-ssd
provisioner: kubernetes.io/no-provisioner
volumeBindingMode: WaitForFirstConsumer
```
