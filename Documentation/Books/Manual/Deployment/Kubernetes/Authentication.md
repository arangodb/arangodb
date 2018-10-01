<!-- don't edit here, it's from https://@github.com/arangodb/kube-arangodb.git / docs/Manual/ -->
# Authentication

The ArangoDB Kubernetes Operator will by default create ArangoDB deployments
that require authentication to access the database.

It uses a single JWT secret (stored in a Kubernetes secret)
to provide *super-user* access between all servers of the deployment
as well as access from the ArangoDB Operator to the deployment.

To disable authentication, set `spec.auth.jwtSecretName` to `None`.

Initially the deployment is accessible through the web user-interface and
APIs, using the user `root` with an empty password.
Make sure to change this password immediately after starting the deployment!

## See also

- [Secure connections (TLS)](./Tls.md)
