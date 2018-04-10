<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Upgrading

The ArangoDB Kubernetes Operator supports upgrading an ArangoDB from
one version to the next.

To upgrade a cluster, change the version by changing
the `spec.image` setting and the apply the updated
custom resource using:

```bash
kubectl apply -f yourCustomResourceFile.yaml
```

To update the ArangoDB Kubernetes Operator itself to a new version,
update the image version of the deployment resource
and apply it using:

```bash
kubectl apply -f examples/yourUpdatedDeployment.yaml
```
