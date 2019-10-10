---
layout: default
description: The ArangoDB Kubernetes Operator needs to be installed in your Kubernetescluster first
---

# Using the ArangoDB Kubernetes Operator

## Installation

The ArangoDB Kubernetes Operator needs to be installed in your Kubernetes
cluster first.

If you have `Helm` available, we recommend installation using `Helm`.

### Installation with Helm

To install the ArangoDB Kubernetes Operator with [`helm`](https://www.helm.sh/){:target="_blank"},
run (replace `<version>` with the version of the operator that you want to install):

```bash
export URLPREFIX=https://github.com/arangodb/kube-arangodb/releases/download/<version>
helm install $URLPREFIX/kube-arangodb-crd.tgz
helm install $URLPREFIX/kube-arangodb.tgz
```

This installs operators for the `ArangoDeployment` and `ArangoDeploymentReplication`
resource types.

If you want to avoid the installation of the operator for the `ArangoDeploymentReplication`
resource type, add `--set=DeploymentReplication.Create=false` to the `helm install`
command.

To use `ArangoLocalStorage` resources, also run:

```bash
helm install $URLPREFIX/kube-arangodb-storage.tgz
```

For more information on installing with `Helm` and how to customize an installation,
see [Using the ArangoDB Kubernetes Operator with Helm](deployment-kubernetes-helm.html).

### Installation with Kubectl

To install the ArangoDB Kubernetes Operator without `Helm`,
run (replace `<version>` with the version of the operator that you want to install):

```bash
export URLPREFIX=https://raw.githubusercontent.com/arangodb/kube-arangodb/<version>/manifests
kubectl apply -f $URLPREFIX/arango-crd.yaml
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
[in the kube-arangodb repository](https://github.com/arangodb/kube-arangodb/releases/latest){:target="_blank"}.

## ArangoDB deployment creation

Once the operator is running, you can create your ArangoDB database deployment
by creating a `ArangoDeployment` custom resource and deploying it into your
Kubernetes cluster.

For example (all examples can be found [in the kube-arangodb repository](https://github.com/arangodb/kube-arangodb/tree/master/examples){:target="_blank"}):

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
helm delete <release-name-of-kube-arangodb-chart>
# If `ArangoLocalStorage` operator is installed
helm delete <release-name-of-kube-arangodb-storage-chart>
```

or when you used `kubectl` to install the operator, run:

```bash
kubectl delete deployment arango-deployment-operator
# If `ArangoLocalStorage` operator is installed
kubectl delete deployment -n kube-system arango-storage-operator
# If `ArangoDeploymentReplication` operator is installed
kubectl delete deployment arango-deployment-replication-operator
```

## See also

- [Driver configuration](deployment-kubernetes-driver-configuration.html)
- [Scaling](deployment-kubernetes-scaling.html)
- [Upgrading](deployment-kubernetes-upgrading.html)
- [Using the ArangoDB Kubernetes Operator with Helm](deployment-kubernetes-helm.html)