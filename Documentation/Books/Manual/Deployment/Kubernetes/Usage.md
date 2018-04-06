<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Using the ArangoDB Kubernetes Operator

## Installation

The ArangoDB Kubernetes Operator needs to be installed in your Kubernetes
cluster first.

To do so, run (replace `<version>` with the version of the operator that you want to install):

```bash
kubectl apply -f https://raw.githubusercontent.com/arangodb/kube-arangodb/<version>/manifests/crd.yaml
kubectl apply -f https://raw.githubusercontent.com/arangodb/kube-arangodb/<version>/manifests/arango-deployment.yaml
```

To use `ArangoLocalStorage`, also run:

```bash
kubectl apply -f https://raw.githubusercontent.com/arangodb/kube-arangodb/<version>/manifests/arango-storage.yaml
```

You can find the latest release of the ArangoDB Kubernetes Operator
[in the kube-arangodb repository](https://github.com/arangodb/kube-arangodb/releases/latest).

## Cluster creation

Once the operator is running, you can create your ArangoDB cluster
by creating a custom resource and deploying it.

For example (all examples can be found [in the kube-arangodb repository](https://github.com/arangodb/kube-arangodb/tree/master/examples)):

```bash
kubectl apply -f examples/simple-cluster.yaml
```

## Cluster removal

To remove an existing cluster, delete the custom
resource. The operator will then delete all created resources.

For example:

```bash
kubectl delete -f examples/simple-cluster.yaml
```

## Operator removal

To remove the entire ArangoDB Kubernetes Operator, remove all
clusters first and then remove the operator by running:

```bash
kubectl delete deployment arango-deployment-operator
# If `ArangoLocalStorage` is installed
kubectl delete deployment -n kube-system arango-storage-operator
```
