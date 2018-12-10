<!-- don't edit here, it's from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Start ArangoDB on Kubernetes in 5 minutes

Starting an ArangoDB database (either single server or full blown cluster)
on Kubernetes involves a lot of resources.

The servers needs to run in `Pods`, you need `Secrets` for authentication,
TLS certificates and `Services` to enable communication with the database.

Use `kube-arangodb`, the ArangoDB Kubernetes Operator to greatly simplify
this process.

In this guide, we will explain what the ArangoDB Kubernetes Operator is,
how to install it and how use it to deploy your first ArangoDB database
in a Kubernetes cluster.

First, you obviously need a Kubernetes cluster and the right credentials
to access it. If you already have this, you can immediately skip to the
next section. Since different cloud providers differ slightly in their
Kubernetes offering, we have put together detailed tutorials for those
platforms we officially support, follow the link for detailed setup
instructions:

 - [Amazon Elastic Kubernetes Service (EKS)](EKS.md)
 - [Google Kubernetes Engine (GKE)](GKE.md)
 - [Microsoft Azure Kubernetes Service (AKS)](AKS.md)

Note that in particular the details of Role Based Access Control (RBAC)
matter.

## What is `kube-arangodb`

`kube-arangodb` is a set of two operators that you deploy in your Kubernetes
cluster to (1) manage deployments of the ArangoDB database and (2)
provide `PersistentVolumes` on local storage of your nodes for optimal
storage performance.

Note that the operator that provides `PersistentVolumes` is not needed to
run ArangoDB deployments. You can also use `PersistentVolumes` provided
by other controllers.

In this guide we will focus on the `ArangoDeployment` operator.

## Installing `kube-arangodb`

To install `kube-arangodb` in your Kubernetes cluster, make sure
you have access to this cluster and the rights to deploy resources
at cluster level.

For now, any recent Kubernetes cluster will do (e.g. `minikube`).

Then run (replace `<version>` with the version of the operator that you want to install):

```bash
kubectl apply -f https://raw.githubusercontent.com/arangodb/kube-arangodb/<version>/manifests/arango-deployment.yaml
# Optional
kubectl apply -f https://raw.githubusercontent.com/arangodb/kube-arangodb/<version>/manifests/arango-storage.yaml
```

The first command installs two `CustomResourceDefinitions` in your Kubernetes cluster:

- `ArangoDeployment` is the resource used to deploy ArangoDB database.
- `ArangoLocalStorage` is the resource used to provision `PersistentVolumes` on local storage.

The second command installs a `Deployment` that runs the operator that controls
`ArangoDeployment` resources.

The optional third command installs a `Deployment` that runs the operator that
provides `PersistentVolumes` on local disks of the cluster nodes.
Use this when running on bare-metal or if there is no provisioner for fast
storage in your Kubernetes cluster.

## Deploying your first ArangoDB database

The first database we are going to deploy is a single server database.

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

To inspect the current status of your deployment, run:

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
available from within the Kubernetes cluster as well as outside it.

Access to the database from outside the Kubernetes cluster is provided
using an external-access service.
By default this service is of type `LoadBalancer`. If this type of service
is not supported by your Kubernetes cluster, it will be replaced by
a service of type `NodePort` after a minute.

To see the type of service that has been created, run:

```bash
kubectl get service single-server-ea
```

When the service is of the `LoadBalancer` type, use the IP address
listed in the `EXTERNAL-IP` column with port 8529.
When the service is of the `NodePort` type, use the IP address
of any of the nodes of the cluster, combine with the high (>30000) port listed in the `PORT(S)` column.

Now you can connect your browser to `https://<ip>:<port>/`.

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

## Where to go from here

- [ArangoDB Kubernetes Operator](../../Deployment/Kubernetes/README.md)
