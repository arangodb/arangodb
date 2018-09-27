<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Using the ArangoDB Kubernetes Operator

## Installation

The ArangoDB Kubernetes Operator needs to be installed in your Kubernetes
cluster first.

To do so, run (replace `<version>` with the version of the operator that you want to install):

```bash
export URLPREFIX=https://raw.githubusercontent.com/arangodb/kube-arangodb/<version>/manifests
kubectl apply -f $URLPREFIX/crd.yaml
kubectl apply -f $URLPREFIX/arango-deployment.yaml
```

To use `ArangoLocalStorage` resources, also run:

```bash
kubectl apply -f $URLPREFIX/arango-storage.yaml
```

To use `ArangoDeploymentReplication` resources, also run:

```bash
kubectl apply -f $URLPREFIX/arango-deployment-replication.yaml
```

You can find the latest release of the ArangoDB Kubernetes Operator
[in the kube-arangodb repository](https://github.com/arangodb/kube-arangodb/releases/latest).

## ArangoDB deployment creation

Once the operator is running, you can create your ArangoDB database deployment
by creating a `ArangoDeployment` custom resource and deploying it into your
Kubernetes cluster.

For example (all examples can be found [in the kube-arangodb repository](https://github.com/arangodb/kube-arangodb/tree/master/examples)):

```bash
kubectl apply -f examples/simple-cluster.yaml
```

## Deployment removal

To remove an existing ArangoDB deployment, delete the custom
resource. The operator will then delete all created resources.

For example:

```bash
kubectl delete -f examples/simple-cluster.yaml
```

**Note that this will also delete all data in your ArangoDB deployment!**

If you want to keep your data, make sure to create a backup before removing the deployment.

## Operator removal

To remove the entire ArangoDB Kubernetes Operator, remove all
clusters first and then remove the operator by running:

```bash
kubectl delete deployment arango-deployment-operator
# If `ArangoLocalStorage` operator is installed
kubectl delete deployment -n kube-system arango-storage-operator
# If `ArangoDeploymentReplication` operator is installed
kubectl delete deployment arango-deployment-replication-operator
```

## See also

- [Driver configuration](./DriverConfiguration.md)
- [Scaling](./Scaling.md)
- [Upgrading](./Upgrading.md)
