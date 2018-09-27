<!-- don't edit here, its from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Configuring your driver for ArangoDB access

In this chapter you'll learn how to configure a driver for accessing
an ArangoDB deployment in Kubernetes.

The exact methods to configure a driver are specific to that driver.

## Database endpoint(s)

The endpoint(s) (or URLs) to communicate with is the most important
parameter your need to configure in your driver.

Finding the right endpoints depend on wether your client application is running in
the same Kubernetes cluster as the ArangoDB deployment or not.

### Client application in same Kubernetes cluster

If your client application is running in the same Kubernetes cluster as
the ArangoDB deployment, you should configure your driver to use the
following endpoint:

```text
https://<deployment-name>.<namespace>.svc:8529
```

Only if your deployment has set `spec.tls.caSecretName` to `None`, should
you use `http` instead of `https`.

### Client application outside Kubernetes cluster

If your client application is running outside the Kubernetes cluster in which
the ArangoDB deployment is running, your driver endpoint depends on the
external-access configuration of your ArangoDB deployment.

If the external-access of the ArangoDB deployment is of type `LoadBalancer`,
then use the IP address of that `LoadBalancer` like this:

```text
https://<load-balancer-ip>:8529
```

If the external-access of the ArangoDB deployment is of type `NodePort`,
then use the IP address(es) of the `Nodes` of the Kubernetes cluster,
combined with the `NodePort` that is used by the external-access service.

For example:

```text
https://<kubernetes-node-1-ip>:30123
```

You can find the type of external-access by inspecting the external-access `Service`.
To do so, run the following command:

```bash
kubectl get service -n <namespace-of-deployment> <deployment-name>-ea
```

The output looks like this:

```bash
NAME                         TYPE           CLUSTER-IP       EXTERNAL-IP      PORT(S)          AGE       SELECTOR
example-simple-cluster-ea    LoadBalancer   10.106.175.38    192.168.10.208   8529:31890/TCP   1s        app=arangodb,arango_deployment=example-simple-cluster,role=coordinator
```

In this case the external-access is of type `LoadBalancer` with a load-balancer IP address
of `192.168.10.208`.
This results in an endpoint of `https://192.168.10.208:8529`.

## TLS settings

As mentioned before the ArangoDB deployment managed by the ArangoDB operator
will use a secure (TLS) connection unless you set `spec.tls.caSecretName` to `None`
in your `ArangoDeployment`.

When using a secure connection, you can choose to verify the server certificates
provides by the ArangoDB servers or not.

If you want to verify these certificates, configure your driver with the CA certificate
found in a Kubernetes `Secret` found in the same namespace as the `ArangoDeployment`.

The name of this `Secret` is stored in the `spec.tls.caSecretName` setting of
the `ArangoDeployment`. If you don't set this setting explicitly, it will be
set automatically.

Then fetch the CA secret using the following command (or use a Kubernetes client library to fetch it):

```bash
kubectl get secret -n <namespace> <secret-name> --template='{{index .data "ca.crt"}}' | base64 -D > ca.crt
```

This results in a file called `ca.crt` containing a PEM encoded, x509 CA certificate.

## Query requests

For most client requests made by a driver, it does not matter if there is any
kind of load-balancer between your client application and the ArangoDB
deployment.

{% hint 'info' %}
Note that even a simple `Service` of type `ClusterIP` already behaves as a
load-balancer.
{% endhint %}

The exception to this is cursor-related requests made to an ArangoDB `Cluster`
deployment. The coordinator that handles an initial query request (that results
in a `Cursor`) will save some in-memory state in that coordinator, if the result
of the query is too big to be transfer back in the response of the initial
request.

Follow-up requests have to be made to fetch the remaining data. These follow-up
requests must be handled by the same coordinator to which the initial request
was made. As soon as there is a load-balancer between your client application
and the ArangoDB cluster, it is uncertain which coordinator will receive the
follow-up request.

ArangoDB will transparently forward any mismatched requests to the correct
coordinator, so the requests can be answered correctly without any additional
configuration. However, this incurs a small latency penalty due to the extra
request across the internal network.

To prevent this uncertainty client-side, make sure to run your client
application in the same Kubernetes cluster and synchronize your endpoints before
making the initial query request. This will result in the use (by the driver) of
internal DNS names of all coordinators. A follow-up request can then be sent to
exactly the same coordinator.

If your client application is running outside the Kubernetes cluster the easiest
way to work around it is by making sure that the query results are small enough
to be returned by a single request. When that is not feasible, it is also
possible to resolve this when the internal DNS names of your Kubernetes cluster
are exposed to your client application and the resulting IP addresses are
routable from your client application. To expose internal DNS names of your
Kubernetes cluster, your can use [CoreDNS](https://coredns.io).
