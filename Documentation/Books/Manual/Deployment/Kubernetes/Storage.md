<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Storage

An ArangoDB cluster relies heavily on fast persistent storage.
The ArangoDB Kubernetes Operator uses `PersistentVolumeClaims` to deliver
the storage to Pods that need them.

## Storage configuration

In the `ArangoDeployment` resource, one can specify the type of storage
used by groups of servers using the `spec.<group>.storageClassName`
setting.

This is an example of a `Cluster` deployment that stores its agent & dbserver
data on `PersistentVolumes` that use the `my-local-ssd` `StorageClass`

```yaml
apiVersion: "database.arangodb.com/v1alpha"
kind: "ArangoDeployment"
metadata:
  name: "cluster-using-local-ssh"
spec:
  mode: Cluster
  agents:
    storageClassName: my-local-ssd
  dbservers:
    storageClassName: my-local-ssd
```

The amount of storage needed is configured using the
`spec.<group>.resources.requests.storage` setting.

Note that configuring storage is done per group of servers.
It is not possible to configure storage per individual
server.

This is an example of a `Cluster` deployment that requests volumes of 80GB
for every dbserver, resulting in a total storage capacity of 240GB (with 3 dbservers).

```yaml
apiVersion: "database.arangodb.com/v1alpha"
kind: "ArangoDeployment"
metadata:
  name: "cluster-using-local-ssh"
spec:
  mode: Cluster
  dbservers:
    resources:
      requests:
        storage: 80Gi
```

## Local storage

For optimal performance, ArangoDB should be configured with locally attached
SSD storage.

The easiest way to accomplish this is to deploy an
[`ArangoLocalStorage` resource](./StorageResource.md).
The ArangoDB Storage Operator will use it to provide `PersistentVolumes` for you.

This is an example of an `ArangoLocalStorage` resource that will result in
`PersistentVolumes` created on any node of the Kubernetes cluster
under the directory `/mnt/big-ssd-disk`.

```yaml
apiVersion: "storage.arangodb.com/v1alpha"
kind: "ArangoLocalStorage"
metadata:
  name: "example-arangodb-storage"
spec:
  storageClass:
    name: my-local-ssd
  localPath:
  - /mnt/big-ssd-disk
```

Note that using local storage required `VolumeScheduling` to be enabled in your
Kubernetes cluster. ON Kubernetes 1.10 this is enabled by default, on version
1.9 you have to enable it with a `--feature-gate` setting.

### Manually creating `PersistentVolumes`

The alternative is to create `PersistentVolumes` manually, for all servers that
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
