<!-- don't edit here, its from https://@github.com/arangodb-helper/arangodb.git / docs/Manual/ -->
# Security

Securing an ArangoDB deployment involves encrypting its connections and
authenticated access control.

The ArangoDB starter provides several command to create the certificates
and tokens needed to do so.

## Creating certificates

The starter provides commands to create all certificates needed for an ArangoDB
deployment with optional datacenter to datacenter replication.

### TLS server certificates

To create a certificate used for TLS servers in the **keyfile** format,
you need the public key of the CA (`--cacert`), the private key of
the CA (`--cakey`) and one or more hostnames (or IP addresses).
Then run:

```bash
arangodb create tls keyfile \
    --cacert=my-tls-ca.crt --cakey=my-tls-ca.key \
    --host=<hostname> \
    --keyfile=my-tls-cert.keyfile
```

Make sure to store the generated keyfile (`my-tls-cert.keyfile`) in a safe place.

To create a certificate used for TLS servers in the **crt** & **key** format,
you need the public key of the CA (`--cacert`), the private key of
the CA (`--cakey`) and one or more hostnames (or IP addresses).
Then run:

```bash
arangodb create tls certificate \
    --cacert=my-tls-ca.crt --cakey=my-tls-ca.key \
    --host=<hostname> \
    --cert=my-tls-cert.crt \
    --key=my-tls-cert.key \
```

Make sure to protect and store the generated files (`my-tls-cert.crt` & `my-tls-cert.key`) in a safe place.

### Client authentication certificates

To create a certificate used for client authentication in the **keyfile** format,
you need the public key of the CA (`--cacert`), the private key of
the CA (`--cakey`) and one or more hostnames (or IP addresses) or email addresses.
Then run:

```bash
arangodb create client-auth keyfile \
    --cacert=my-client-auth-ca.crt --cakey=my-client-auth-ca.key \
    [--host=<hostname> | --email=<emailaddress>] \
    --keyfile=my-client-auth-cert.keyfile
```

Make sure to protect and store the generated keyfile (`my-client-auth-cert.keyfile`) in a safe place.

### CA certificates

To create a CA certificate used to **sign TLS certificates**, run:

```bash
arangodb create tls ca \
    --cert=my-tls-ca.crt --key=my-tls-ca.key
```

Make sure to protect and store both generated files (`my-tls-ca.crt` & `my-tls-ca.key`) in a safe place.

Note: CA certificates have a much longer lifetime than normal certificates.
Therefore even more care is needed to store them safely.

To create a CA certificate used to **sign client authentication certificates**, run:

```bash
arangodb create client-auth ca \
    --cert=my-client-auth-ca.crt --key=my-client-auth-ca.key
```

Make sure to protect and store both generated files (`my-client-auth-ca.crt` & `my-client-auth-ca.key`)
in a safe place.

Note: CA certificates have a much longer lifetime than normal certificates.
Therefore even more care is needed to store them safely.

## Creating authentication tokens

JWT tokens are used to authenticate servers (within a cluster) with each other.

### JWT tokens

To create a file containing an JWT token, run:

```bash
arangodb create jwt-secret \
    --secret=my-secret.jwt [--length=32]
```

Make sure to protect and store the generated file (`my-secret.jwt`) in a safe place.

## Using authentication tokens

ArangoDB deployments that require authentication can be accessed through standard user+password
pairs or using a JWT to get "super-user" access.

This super-user access is needed to communicate directly with the agency or with any server
in the deployment.
Note that uses super-user access for normal database access is NOT advised.

To create a JWT from the JWT secret file specified using the `--auth.jwt-secret` option,
use the following command:

```bash
arangodb auth token --auth.jwt-secret=<secret-file>
```

To create a complete HTTP Authorization header that can be passed directly to tools like `curl`,
use the following command:

```bash
arangodb auth header --auth.jwt-secret=<secret-file>
```

Using `curl` with this command looks like this:

```bash
curl -v -H "$(arangodb auth header --auth.jwt-secret=<secret-file>)" http://<database-ip>:8529/_api/version
```

Note the double quotes around `$(...)`.
