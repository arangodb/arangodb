<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# ArangoDB Kubernetes Operator

The ArangoDB Kubernetes Operator (`kube-arangodb`) is a set of operators
that you deploy in your Kubernetes cluster to:

- Manage deployments of the ArangoDB database
- Provide `PersistentVolumes` on local storage of your nodes for optimal storage performance.
- Configure ArangoDB Datacenter to Datacenter replication

Each of these uses involves a different custom resource.

- Use an [`ArangoDeployment` resource](./DeploymentResource.md) to
  create an ArangoDB database deployment.
- Use an [`ArangoLocalStorage` resource](./StorageResource.md) to
  provide local `PersistentVolumes` for optimal I/O performance.
- Use an [`ArangoDeploymentReplication` resource](./DeploymentReplicationResource.md) to
  configure ArangoDB Datacenter to Datacenter replication.

Continue with [Using the ArangoDB Kubernetes Operator](./Usage.md)
to learn how to install the ArangoDB Kubernetes operator and create
your first deployment.
