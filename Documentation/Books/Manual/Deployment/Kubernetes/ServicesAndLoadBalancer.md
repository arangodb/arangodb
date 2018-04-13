<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Services and load balancer

The ArangoDB Kubernetes Operator will create services that can be used to
reach the ArangoDB servers from inside the Kubernetes cluster.

To use the ArangoDB servers from outside the Kubernetes cluster
you have to add another service as explained below.

## Services

### Single server

For a single server deployment, the operator creates a single
`Service` named `<cluster-name>`. This service has a normal cluster IP
address.

### Full cluster

For a full cluster deployment, the operator creates two `Services`.

- `<cluster-name>_servers` a headless `Service` intended to provide
  DNS names for all pods created by the operator.
  It selects all ArangoDB & ArangoSync servers in the cluster.

- `<cluster-name>` a normal `Service` that selects only the coordinators
  of the cluster. This `Service` is configured with `ClientIP` session
  affinity. This is needed for cursor requests, since they are bound to
  a specific coordinator.

When the coordinators are asked to provide endpoints of the cluster
(e.g. when calling `client.SynchronizeEndpoints()` in the go driver)
the DNS names of the individual `Pods` will be returned
(`<pod>.<cluster-name>_servers.<namespace>.svc`)

### Full cluster with DC2DC

For a full cluster with datacenter replication deployment,
the same `Services` are created as for a Full cluster, with the following
additions:

- `<cluster-name>_sync` a normal `Service` that selects only the syncmasters
  of the cluster.

## Load balancer

To reach the ArangoDB servers from outside the Kubernetes cluster, you
have to deploy additional services.

You can use `LoadBalancer` or `NodePort` services, depending on your
Kubernetes deployment.

This service should select:

- `arangodb_cluster_name: <cluster-name>`
- `role: coordinator`

For example:

```yaml
kind: Service
apiVersion: v1
metadata:
  name: arangodb-cluster-exposed
spec:
  selector:
    arangodb_cluster_name: arangodb-cluster
    role: coordinator
  type: NodePort
  ports:
  - protocol: TCP
    port: 8529
    targetPort: 8529
    nodePort: 30529
```
