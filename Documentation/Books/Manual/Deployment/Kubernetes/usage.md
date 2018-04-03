<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Using the ArangoDB operator

## Installation

The ArangoDB operator needs to be installed in your Kubernetes
cluster first. To do so, clone this repository and run:

```bash
kubectl apply -f manifests/crd.yaml
kubectl apply -f manifests/arango-deployment.yaml
```

To use `ArangoLocalStorage`, also run:

```bash
kubectl apply -f manifests/arango-storage.yaml
```

## Cluster creation

Once the operator is running, you can create your ArangoDB cluster
by creating a custom resource and deploying it.

For example:

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

To remove the entire ArangoDB operator, remove all
clusters first and then remove the operator by running:

```bash
kubectl delete -f manifests/arango-deployment.yaml
# If `ArangoLocalStorage` is installed
kubectl delete -f manifests/arango-storage.yaml
```
