<!-- don't edit here, it's from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# ArangoDeploymentReplication Custom Resource

The ArangoDB Replication Operator creates and maintains ArangoDB
`arangosync` configurations in a Kubernetes cluster, given a replication specification.
This replication specification is a `CustomResource` following
a `CustomResourceDefinition` created by the operator.

Example minimal replication definition for 2 ArangoDB cluster with sync in the same Kubernetes cluster:

```yaml
apiVersion: "replication.database.arangodb.com/v1alpha"
kind: "ArangoDeploymentReplication"
metadata:
  name: "replication-from-a-to-b"
spec:
  source:
    deploymentName: cluster-a
    auth:
      keyfileSecretName: cluster-a-sync-auth
  destination:
    deploymentName: cluster-b
```

This definition results in:

- the arangosync `SyncMaster` in deployment `cluster-b` is called to configure a synchronization
  from the syncmasters in `cluster-a` to the syncmasters in `cluster-b`,
  using the client authentication certificate stored in `Secret` `cluster-a-sync-auth`.
  To access `cluster-a`, the JWT secret found in the deployment of `cluster-a` is used.
  To access `cluster-b`, the JWT secret found in the deployment of `cluster-b` is used.

Example replication definition for replicating from a source that is outside the current Kubernetes cluster
to a destination that is in the same Kubernetes cluster:

```yaml
apiVersion: "replication.database.arangodb.com/v1alpha"
kind: "ArangoDeploymentReplication"
metadata:
  name: "replication-from-a-to-b"
spec:
  source:
    masterEndpoint: ["https://163.172.149.229:31888", "https://51.15.225.110:31888", "https://51.15.229.133:31888"]
    auth:
      keyfileSecretName: cluster-a-sync-auth
    tls:
      caSecretName: cluster-a-sync-ca
  destination:
    deploymentName: cluster-b
```

This definition results in:

- the arangosync `SyncMaster` in deployment `cluster-b` is called to configure a synchronization
  from the syncmasters located at the given list of endpoint URLs to the syncmasters `cluster-b`,
  using the client authentication certificate stored in `Secret` `cluster-a-sync-auth`.
  To access `cluster-a`, the keyfile (containing a client authentication certificate) is used.
  To access `cluster-b`, the JWT secret found in the deployment of `cluster-b` is used.

## Specification reference

Below you'll find all settings of the `ArangoDeploymentReplication` custom resource.

### `spec.source.deploymentName: string`

This setting specifies the name of an `ArangoDeployment` resource that runs a cluster
with sync enabled.

This cluster configured as the replication source.

### `spec.source.masterEndpoint: []string`

This setting specifies zero or more master endpoint URLs of the source cluster.

Use this setting if the source cluster is not running inside a Kubernetes cluster
that is reachable from the Kubernetes cluster the `ArangoDeploymentReplication` resource is deployed in.

Specifying this setting and `spec.source.deploymentName` at the same time is not allowed.

### `spec.source.auth.keyfileSecretName: string`

This setting specifies the name of a `Secret` containing a client authentication certificate called `tls.keyfile` used to authenticate
with the SyncMaster at the specified source.

If `spec.source.auth.userSecretName` has not been set,
the client authentication certificate found in the secret with this name is also used to configure
the synchronization and fetch the synchronization status.

This setting is required.

### `spec.source.auth.userSecretName: string`

This setting specifies the name of a `Secret` containing a `username` & `password` used to authenticate
with the SyncMaster at the specified source in order to configure synchronization and fetch synchronization status.

The user identified by the username must have write access in the `_system` database of the source ArangoDB cluster.

### `spec.source.tls.caSecretName: string`

This setting specifies the name of a `Secret` containing a TLS CA certificate `ca.crt` used to verify
the TLS connection created by the SyncMaster at the specified source.

This setting is required, unless `spec.source.deploymentName` has been set.

### `spec.destination.deploymentName: string`

This setting specifies the name of an `ArangoDeployment` resource that runs a cluster
with sync enabled.

This cluster configured as the replication destination.

### `spec.destination.masterEndpoint: []string`

This setting specifies zero or more master endpoint URLs of the destination cluster.

Use this setting if the destination cluster is not running inside a Kubernetes cluster
that is reachable from the Kubernetes cluster the `ArangoDeploymentReplication` resource is deployed in.

Specifying this setting and `spec.destination.deploymentName` at the same time is not allowed.

### `spec.destination.auth.keyfileSecretName: string`

This setting specifies the name of a `Secret` containing a client authentication certificate called `tls.keyfile` used to authenticate
with the SyncMaster at the specified destination.

If `spec.destination.auth.userSecretName` has not been set,
the client authentication certificate found in the secret with this name is also used to configure
the synchronization and fetch the synchronization status.

This setting is required, unless `spec.destination.deploymentName` or `spec.destination.auth.userSecretName` has been set.

Specifying this setting and `spec.destination.userSecretName` at the same time is not allowed.

### `spec.destination.auth.userSecretName: string`

This setting specifies the name of a `Secret` containing a `username` & `password` used to authenticate
with the SyncMaster at the specified destination in order to configure synchronization and fetch synchronization status.

The user identified by the username must have write access in the `_system` database of the destination ArangoDB cluster.

Specifying this setting and `spec.destination.keyfileSecretName` at the same time is not allowed.

### `spec.destination.tls.caSecretName: string`

This setting specifies the name of a `Secret` containing a TLS CA certificate `ca.crt` used to verify
the TLS connection created by the SyncMaster at the specified destination.

This setting is required, unless `spec.destination.deploymentName` has been set.

## Authentication details

The authentication settings in a `ArangoDeploymentReplication` resource are used for two distinct purposes.

The first use is the authentication of the syncmasters at the destination with the syncmasters at the source.
This is always done using a client authentication certificate which is found in a `tls.keyfile` field
in a secret identified by `spec.source.auth.keyfileSecretName`.

The second use is the authentication of the ArangoDB Replication operator with the syncmasters at the source
or destination. These connections are made to configure synchronization, stop configuration and fetch the status
of the configuration.
The method used for this authentication is derived as follows (where `X` is either `source` or `destination`):

- If `spec.X.userSecretName` is set, the username + password found in the `Secret` identified by this name is used.
- If `spec.X.keyfileSecretName` is set, the client authentication certificate (keyfile) found in the `Secret` identifier by this name is used.
- If `spec.X.deploymentName` is set, the JWT secret found in the deployment is used.

## Creating client authentication certificate keyfiles

The client authentication certificates needed for the `Secrets` identified by `spec.source.auth.keyfileSecretName` & `spec.destination.auth.keyfileSecretName`
are normal ArangoDB keyfiles that can be created by the `arangosync create client-auth keyfile` command.
In order to do so, you must have access to the client authentication CA of the source/destination.

If the client authentication CA at the source/destination also contains a private key (`ca.key`), the ArangoDeployment operator
can be used to create such a keyfile for you, without the need to have `arangosync` installed locally.
Read the following paragraphs for instructions on how to do that.

## Creating and using access packages

An access package is a YAML file that contains:

- A client authentication certificate, wrapped in a `Secret` in a `tls.keyfile` data field.
- A TLS certificate authority public key, wrapped in a `Secret` in a `ca.crt` data field.

The format of the access package is such that it can be inserted into a Kubernetes cluster using the standard `kubectl` tool.

To create an access package that can be used to authenticate with the ArangoDB SyncMasters of an `ArangoDeployment`,
add a name of a non-existing `Secret` to the `spec.sync.externalAccess.accessPackageSecretNames` field of the `ArangoDeployment`.
In response, a `Secret` is created in that Kubernetes cluster, with the given name, that contains a `accessPackage.yaml` data field
that contains a Kubernetes resource specification that can be inserted into the other Kubernetes cluster.

The process for creating and using an access package for authentication at the source cluster is as follows:

- Edit the `ArangoDeployment` resource of the source cluster, set `spec.sync.externalAccess.accessPackageSecretNames` to `["my-access-package"]`
- Wait for the `ArangoDeployment` operator to create a `Secret` named `my-access-package`.
- Extract the access package from the Kubernetes source cluster using:

```bash
kubectl get secret my-access-package --template='{{index .data "accessPackage.yaml"}}' | base64 -D > accessPackage.yaml
```

- Insert the secrets found in the access package in the Kubernetes destination cluster using:

```bash
kubectl apply -f accessPackage.yaml
```

As a result, the destination Kubernetes cluster will have 2 additional `Secrets`. One contains a client authentication certificate
formatted as a keyfile. Another contains the public key of the TLS CA certificate of the source cluster.
