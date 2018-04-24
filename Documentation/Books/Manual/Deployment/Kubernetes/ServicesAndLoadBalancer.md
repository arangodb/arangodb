<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Services and load balancer

The ArangoDB Kubernetes Operator will create services that can be used to
reach the ArangoDB servers from inside the Kubernetes cluster.

By default, the ArangoDB Kubernetes Operator will also create an additional
service to reach the ArangoDB deployment from outside the Kubernetes cluster.

For exposing the ArangoDB deployment to the outside, there are 2 options:

- Using a `NodePort` service. This will expose the deployment on a specific port (above 30.000)
  on all nodes of the Kubernetes cluster.
- Using a `LoadBalancer` service. This will expose the deployment on a load-balancer
  that is provisioned by the Kubernetes cluster.

The `LoadBalancer` option is the most convenient, but not all Kubernetes clusters
are able to provision a load-balancer. Therefore we offer a third (and default) option: `Auto`.
In this option, the ArangoDB Kubernetes Operator tries to create a `LoadBalancer`
service. It then waits for up to a minute for the Kubernetes cluster to provision
a load-balancer for it. If that has not happened after a minute, the service
is replaced by a service of type `NodePort`.

To inspect the created service, run:

```bash
kubectl get services <deployment-name>-ea
```

To use the ArangoDB servers from outside the Kubernetes cluster
you have to add another service as explained below.

## Services

If you do not want the ArangoDB Kubernetes Operator to create an external-access
service for you, set `spec.externalAccess.Type` to `None`.

If you want to create external access services manually, follow the instructions below.

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
