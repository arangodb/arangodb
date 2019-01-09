<!-- don't edit here, it's from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Start ArangoDB on Google Kubernetes Engine (GKE)

In this guide you'll learn how to run ArangoDB on Google Kubernetes Engine (GKE).

## Create a Kubernetes cluster

In order to run ArangoDB on GKE you first need to create a Kubernetes cluster.

To do so, go to the GKE console.
You'll find a list of existing clusters (initially empty).

![clusters](./gke-clusters.png)

Click on `CREATE CLUSTER`.

In the form that follows, enter information as seen in the screenshot below.

![create a cluster](./gke-create-cluster.png)

We have successfully ran clusters with 4 `1 vCPU` nodes or 3 `2 vCPU` nodes.
Smaller node configurations will likely lead to unschedulable `Pods`.

Once you click `Create`, you'll return to the list of clusters and your
new cluster will be listed there.

![with new cluster](./gke-clusters-added.png)

It will take a few minutes for the cluster to be created.

Once you're cluster is ready, a `Connect` button will appear in the list.

![cluster is ready](./gke-clusters-ready.png)

## Getting access to your Kubernetes cluster

Once your cluster is ready you must get access to it.
The standard `Connect` button provided by GKE will give you access with only limited
permissions. Since the Kubernetes operator also requires some cluster wide
permissions, you need "administrator" permissions.

To get these permissions, do the following.

Prepare your `~/.kube/config` with the following content:

```yaml
apiVersion: v1
clusters:
- cluster:
    certificate-authority-data:
    server: https://
  name: my_cluster
contexts:
- context:
    cluster: my_cluster
    user: my_cluster
  name: my_cluster
current-context: my_cluster
kind: Config
preferences: {}
users:
- name: my_cluster
  user:
    username: admin
    password:
```

Click on the `Edit` button (pencil image) next to your cluster.
This will lead you to the following page.

![edit cluster](./gke-edit-cluster.png)

Copy the `Endpoint` IP address and paste it after `server: https://`.

The click on `Show credentials`.
The following popup will appear.

![show credentials](./gke-show-credentials.png)

Copy the `Password` and paste it after `password:`.

Close the popup and then return to the cluster list.

Click on `Connect` next to your cluster.
The following popup will appear.

![connect to cluster](./gke-connect-to-cluster.png)

Click on `Run in Cloud Shell`.
It will take some time to launch a shell (in your browser).

Once ready, run the `gcloud` command that is already prepare in your commandline.

The run `cat ~/.kube/config` and copy the line that starts with `certificate-authority-data:`.
Override the corresponding line in your local `~/.kube.config` file.

You should now be able to access your cluster using `kubectl`.

To verify try a command like:

```bash
kubectl get pods --all-namespaces
```

## Installing `kube-arangodb`

You can now install the ArangoDB Kubernetes operator in your Kubernetes cluster
on GKE.

To do so, follow the [Installing kube-arangodb](./README.md#installing-kube-arangodb) instructions.

## Deploying your first ArangoDB database

Once the ArangoDB Kubernetes operator has been installed and its `Pods` are in the `Ready`
state, you can launch your first ArangoDB deployment in your Kubernetes cluster
on GKE.

To do so, follow the [Deploying your first ArangoDB database](./README.md#deploying-your-first-arangodb-database) instructions.

Note that GKE supports `Services` of type `LoadBalancer`.
