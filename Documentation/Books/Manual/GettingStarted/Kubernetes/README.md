<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Start ArangoDB on Kubernetes in 5min

Starting an ArangoDB database (either single server or full blown cluster)
on Kubernetes involves a lot of resources.

The servers needs to run in `Pods`, you need `Secrets` for authentication,
TLS certificates and `Services` to enable communication with the database.

Use `kube-arangodb`, the ArangoDB Kubernetes operator to greatly simplify
this process.

In this guide, we'll explain what the ArangoDB Kubernetes operator is,
how to install it and how use it to deploy your first ArangoDB database
in a Kubernetes cluster.

## What is `kube-arangodb`

`kube-arangodb` is a set of two operators that you deploy in your Kubernetes
cluster to (1) manage deployments of the ArangoDB database and (2)
provide `PersistenVolumes` on local storage of your nodes for optimal
storage performace.

Note that the operator that provides `PersistentVolumes` is not needed to
run ArangoDB deployments. You can also use `PersistentVolumes` provided
by other controllers.

In this guide we'll focus on the `ArangoDeployment` operator.

## Installing `kube-arangodb`

To install `kube-arangodb` in your Kubernetes cluster, make sure
you have acces to this cluster and the rights to deploy resources
at cluster level.

For now, any recent Kubernetes cluster will do (e.g. `minikube`).

Then run (replace `<version>` with the version of the operator that you want to install):

```bash
kubectl apply -f https://raw.githubusercontent.com/arangodb/kube-arangodb/<version>/manifests/crd.yaml
kubectl apply -f https://raw.githubusercontent.com/arangodb/kube-arangodb/<version>/manifests/arango-deployment.yaml
```

The first command installs two `CustomResourceDefinitions` in your Kubernetes cluster:

- `ArangoDeployment` is the resource used to deploy ArangoDB database.
- `ArangoLocalStorage` is the resource used to provision `PersistentVolumes` on local storage.

The second command installs a `Deployment` that runs the operator that controls
`ArangoDeployment` resources.

## Deploying your first ArangoDB database

The first database we're going to deploy is a single server database.

Create a file called `single-server.yaml` with the following content.

```yaml
apiVersion: "database.arangodb.com/v1alpha"
kind: "ArangoDeployment"
metadata:
  name: "single-server"
spec:
  mode: Single
```

Now insert this resource in your Kubernetes cluster using:

```bash
kubectl apply -f single-server.yaml
```

The `ArangoDeployment` operator in `kube-arangodb` will now inspect the
resource you just deployed and start the process to run a single server database.

To inspect the currentl status of your deployment, run:

```bash
kubectl describe ArangoDeployment single-server
# or shorter
kubectl describe arango single-server
```

To inspect the pods created for this deployment, run:

```bash
kubectl get pods --selector=arango_deployment=single-server
```

The result will look similar to this:

```plain
NAME                                 READY     STATUS    RESTARTS   AGE
single-server-sngl-cjtdxrgl-fe06f0   1/1       Running   0          1m
```

Once the pod reports that it is has a `Running` status and is ready,
your database s available.

## Connecting to your database

The single server database you deployed in the previous chapter is now
available, but only from within the Kubernetes cluster.

To make the database available outside your Kubernetes cluster (e.g. for browser acces)
you must deploy an additional `Service`.

There are several possible types of `Service` to choose from.
We're going to use the `NodePort` type to expose the database on port 30529 of
every node of your Kubernetes cluster.

Create a file called `single-server-service.yaml` with the following content.

```yaml
kind: Service
apiVersion: v1
metadata:
  name: single-server-service
spec:
  selector:
    app: arangodb
    arango_deployment: single-server
    role: single
  type: NodePort
  ports:
  - protocol: TCP
    port: 8529
    targetPort: 8529
    nodePort: 30529
```

Deploy the `Service` into your Kubernetes cluster using:

```bash
kubectl apply -f single-server-service.yaml
```

Now you can connect your browser to `https://<node name>:30529/`,
where `<node name>` is the name or IP address of any of the nodes
of your Kubernetes cluster.

Your browser will show a warning about an unknown certificate.
Accept the certificate for now.

Then login using username `root` and an empty password.

If you want to delete your single server ArangoDB database, just run:

```bash
kubectl delete ArangoDeployment single-server
```

## Deploying a full blown ArangoDB cluster database

The deployment of a full blown cluster is very similar to deploying
a single server database. The difference is in the `mode` field of
the `ArangoDeployment` specification.

Create a file called `cluster.yaml` with the following content.

```yaml
apiVersion: "database.arangodb.com/v1alpha"
kind: "ArangoDeployment"
metadata:
  name: "cluster"
spec:
  mode: Cluster
```

Now insert this resource in your Kubernetes cluster using:

```bash
kubectl apply -f cluster.yaml
```

The same commands used in the single server deployment can be used
to inspect your cluster. Just use the correct deployment name (`cluster` instead of `single-server`).

Connecting to your cluster requires a different `Service` since the
selector now has to select your `cluster` deployment and instead
of selecting all `Pods` with role `single` it will have to select
all coordinator pods.

The service looks like this:

```yaml
kind: Service
apiVersion: v1
metadata:
  name: cluster-service
spec:
  selector:
    app: arangodb
    arango_deployment: cluster
    role: coordinator
  type: NodePort
  ports:
  - protocol: TCP
    port: 8529
    targetPort: 8529
    nodePort: 31529
```

Note that we've choosen a different node port (31529) for this `Service`
to avoid conflicts with the port used in `single-server-service`.

## Where to go from here

- [Reference manual](../../Deployment/Kubernetes/README.md)
