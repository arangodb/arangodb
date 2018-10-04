<!-- don't edit here, it's from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# ArangoLocalStorage Custom Resource

The ArangoDB Storage Operator creates and maintains ArangoDB
storage resources in a Kubernetes cluster, given a storage specification.
This storage specification is a `CustomResource` following
a `CustomResourceDefinition` created by the operator.

Example minimal storage definition:

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

This definition results in:

- a `StorageClass` called `my-local-ssd`
- the dynamic provisioning of PersistentVolume's with
  a local volume on a node where the local volume starts
  in a sub-directory of `/mnt/big-ssd-disk`.
- the dynamic cleanup of PersistentVolume's (created by
  the operator) after one is released.

The provisioned volumes will have a capacity that matches
the requested capacity of volume claims.

## Specification reference

Below you'll find all settings of the `ArangoLocalStorage` custom resource.

### `spec.storageClass.name: string`

This setting specifies the name of the storage class that
created `PersistentVolume` will use.

If empty, this field defaults to the name of the `ArangoLocalStorage`
object.

If a `StorageClass` with given name does not yet exist, it
will be created.

### `spec.storageClass.isDefault: bool`

This setting specifies if the created `StorageClass` will
be marked as default storage class. (default is `false`)

### `spec.localPath: stringList`

This setting specifies one of more local directories
(on the nodes) used to create persistent volumes in.

### `spec.nodeSelector: nodeSelector`

This setting specifies which nodes the operator will
provision persistent volumes on.
