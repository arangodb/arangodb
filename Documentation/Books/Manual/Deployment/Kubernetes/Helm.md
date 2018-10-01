<!-- don't edit here, it's from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Using the ArangoDB Kubernetes Operator with Helm

[`Helm`](https://www.helm.sh/) is a package manager for Kubernetes, which enables
you to install various packages (include the ArangoDB Kubernetes Operator)
into your Kubernetes cluster.

The benefit of `helm` (in the context of the ArangoDB Kubernetes Operator)
is that it allows for a lot of flexibility in how you install the operator.
For example you can install the operator in a namespace other than
`default`.

## Charts

The ArangoDB Kubernetes Operator is contained in two `helm` charts:

- `kube-arangodb` which contains the operator for the `ArangoDeployment`
  and `ArangoDeploymentReplication` resource types.
- `kube-arangodb-storage` which contains the operator for the `ArangoLocalStorage`
  resource type.

The `kube-arangodb-storage` only has to be installed if your Kubernetes cluster
does not already provide `StorageClasses` that use locally attached SSDs.

## Configurable values for ArangoDB Kubernetes Operator

The following values can be configured when installing the
ArangoDB Kubernetes Operator with `helm`.

Values are passed to `helm` using an `--set=<key>=<value>` argument passed
to the `helm install` or `helm upgrade` command.

### Values applicable to both charts

| Key               | Type   | Description
|-------------------|--------|-----|
| Image             | string | Override the docker image used by the operators
| ImagePullPolicy   | string | Override the image pull policy used by the operators. See [Updating Images](https://kubernetes.io/docs/concepts/containers/images/#updating-images) for details.
| RBAC.Create       | bool   | Set to `true` (default) to create roles & role bindings.

### Values applicable to the `kube-arangodb` chart

| Key               | Type   | Description
|-------------------|--------|-----|
| Deployment.Create | bool   | Set to `true` (default) to deploy the `ArangoDeployment` operator
| Deployment.User.ServiceAccountName | string | Name of the `ServiceAccount` that is the subject of the `RoleBinding` of users of the `ArangoDeployment` operator
| Deployment.Operator.ServiceAccountName | string | Name of the `ServiceAccount` used to run the `ArangoDeployment` operator
| Deployment.Operator.ServiceType | string | Type of `Service` created for the dashboard of the `ArangoDeployment` operator
| Deployment.AllowChaos | bool | Set to `true` to allow the introduction of chaos. **Only use for testing, never for production!** Defaults to `false`.
| DeploymentReplication.Create | bool   | Set to `true` (default) to deploy the `ArangoDeploymentReplication` operator
| DeploymentReplication.User.ServiceAccountName | string | Name of the `ServiceAccount` that is the subject of the `RoleBinding` of users of the `ArangoDeploymentReplication` operator
| DeploymentReplication.Operator.ServiceAccountName | string | Name of the `ServiceAccount` used to run the `ArangoDeploymentReplication` operator
| DeploymentReplication.Operator.ServiceType | string | Type of `Service` created for the dashboard of the `ArangoDeploymentReplication` operator

### Values applicable to the `kube-arangodb-storage` chart

| Key               | Type   | Description
|-------------------|--------|-----|
| Storage.User.ServiceAccountName | string | Name of the `ServiceAccount` that is the subject of the `RoleBinding` of users of the `ArangoLocalStorage` operator
| Storage.Operator.ServiceAccountName | string | Name of the `ServiceAccount` used to run the `ArangoLocalStorage` operator
| Storage.Operator.ServiceType | string | Type of `Service` created for the dashboard of the `ArangoLocalStorage` operator

## Alternate namespaces

The `kube-arangodb` chart supports deployment into a non-default namespace.

To install the `kube-arangodb` chart is a non-default namespace, use the `--namespace`
argument like this.

```bash
helm install --namespace=mynamespace kube-arangodb.tgz
```

Note that since the operators claim exclusive access to a namespace, you can
install the `kube-arangodb` chart in a namespace once.
You can install the `kube-arangodb` chart in multiple namespaces. To do so, run:

```bash
helm install --namespace=namespace1 kube-arangodb.tgz
helm install --namespace=namespace2 kube-arangodb.tgz
```

The `kube-arangodb-storage` chart is always installed in the `kube-system` namespace.

## Common problems

### Error: no available release name found

This error is given by `helm install ...` in some cases where it has
insufficient permissions to install charts.

For various ways to work around this problem go to [this Stackoverflow article](https://stackoverflow.com/questions/43499971/helm-error-no-available-release-name-found).
