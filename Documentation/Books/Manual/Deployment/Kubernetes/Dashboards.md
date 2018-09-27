<!-- don't edit here, it's from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Operator Dashboards

The ArangoDB Kubernetes Operator can create a dashboard for each type of
resource it supports. These dashboards are intended to give an overview of
the created resources, their state and instructions on how to modify those resources.

The dashboards do not provide direct means to modify the resources.
All modifications are done using `kubectl` commands (which are provided by the dashboards)
so the standard security of your Kubernetes cluster is not bypassed.

## Exposing the dashboards

For each resource type (deployment, deployment replication & local storage) operator
a `Service` is created that serves the dashboard internally in the Kubernetes cluster.
To expose a dashboard outside the Kubernetes cluster, run a `kubecty expose`
command like this:

```bash
kubectl expose service <service-name> --type=LoadBalancer \
  --port=8528 --target-port=8528 \
  --name=<your-exposed-service-name> --namespace=<the-namespace>
```

Replace `<service-name>` with:

- `arango-deployment-operator` for the ArangoDeployment operator dashboard.
- `arango-deployment-replication-operator` for the ArangoDeploymentReplication
   operator dashboard.
- `arango-storage-operator` for the ArangoLocalStorage operator dashboard.
   (use 'kube-system' namespace)

Replace `<the-namespace>` with the name of the namespace that the operator is in.
This will often be `default`.

This will create an additional `Service` of type `LoadBalancer` that copies
the selector from the existing `Service`.
If your Kubernetes cluster does not support loadbalancers,
use `--type=NodePort` instead.

Run the following command to inspect your new service and look for the
loadbalancer IP/host address (or nodeport).

```bash
kubectl get service <your-exposed-service-name> --namespace=<the-namespace>
```

This will result in something like this:

```bash
$ kubectl get service arango-storage-operator-lb --namespace=kube-system
NAME                         TYPE           CLUSTER-IP     EXTERNAL-IP     PORT(S)          AGE
arango-storage-operator-lb   LoadBalancer   10.103.30.24   192.168.31.11   8528:30655/TCP   1d
```

## Authentication

While the dashboards do not provide any means to directly modify resources,
they still show sensitive information (e.g. TLS certificates).
Therefore the dashboards require a username+password for authentications.

The username+password pair is configured in a generic Kubernetes `Secret` named `arangodb-operator-dashboard`, found in the namespace where the operator runs.

To create such a secret, run this:

```bash
kubectl create secret generic \
  arangodb-operator-dashboard --namespace=<the-namespace> \
  --from-literal=username=<username> \
  --from-literal=password=<password>
```

Until such a `Secret` is found, the operator will respond with a status `401`
to any request related to the dashboard.
