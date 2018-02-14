<!-- don't edit here, its from https://@github.com//arangodb-helper/arangodb.git / docs/Manual/ -->
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
