<!-- don't edit here, its from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Datacenter to datacenter Security

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

This section includes information related to the _datacenter to datacenter replication_
security.

For a general introduction to the _datacenter to datacenter replication_, please
refer to the [Datacenter to datacenter replication](../../Architecture/DeploymentModes/DC2DC/README.md)
chapter.

## Firewall settings

The components of _ArangoSync_ use (TCP) network connections to communicate with each other.
Below you'll find an overview of these connections and the TCP ports that should be accessible.

1. The sync masters must be allowed to connect to the following components
   within the same datacenter:

   - ArangoDB agents and coordinators (default ports: `8531` and `8529`)
   - Kafka brokers (only when using `kafka` type message queue) (default port `9092`)
   - Sync workers (default port `8729`)

   Additionally the sync masters must be allowed to connect to the sync masters in the other datacenter.

   By default the sync masters will operate on port `8629`.

1. The sync workers must be allowed to connect to the following components within the same datacenter:

   - ArangoDB coordinators (default port `8529`)
   - Kafka brokers (only when using `kafka` type message queue) (default port `9092`)
   - Sync masters (default port `8629`)

   By default the sync workers will operate on port `8729`.

   Additionally (when using `kafka` type message queue) the sync workers must be allowed to
   connect to the Kafka brokers in the other datacenter.

1. Kafka (when using `kafka` type message queue)

   The kafka brokers must be allowed to connect to the following components within the same datacenter:

   - Other kafka brokers (default port `9092`)
   - Zookeeper (default ports `2181`, `2888` and `3888`)

   The default port for kafka is `9092`. The default kafka installation will also expose some prometheus
   metrics on port `7071`. To gain more insight into kafka open this port for your prometheus
   installation.

1. Zookeeper (when using `kafka` type message queue)

   The zookeeper agents must be allowed to connect to the following components within the same datacenter:

   - Other zookeeper agents

   The setup here is a bit special as zookeeper uses 3 ports for different operations. All agents need to
   be able to connect to all of these ports.

   By default Zookeeper uses:

   - port `2181` for client communication
   - port `2888` for follower communication
   - port `3888` for leader elections

## Certificates

Digital certificates are used in many places in _ArangoSync_ for both encryption
and authentication.

<br/> In ArangoSync all network connections are using Transport Layer Security (TLS),
a set of protocols that ensure that all network traffic is encrypted.
For this TLS certificates are used. The server side of the network connection
offers a TLS certificate. This certificate is (often) verified by the client side of the network
connection, to ensure that the certificate is signed by a trusted Certificate Authority (CA).
This ensures the integrity of the server.
<br/> In several places additional certificates are used for authentication. In those cases
the client side of the connection offers a client certificate (on top of an existing TLS connection).
The server side of the connection uses the client certificate to authenticate
the client and (optionally) decides which rights should be assigned to the client.

Note: ArangoSync does allow the use of certificates signed by a well know CA (eg. verisign)
however it is more convenient (and common) to use your own CA.

### Formats

All certificates are x509 certificates with a public key, a private key and
an optional chain of certificates used to sign the certificate. This chain is
typically provided by the Certificate Authority (CA).
<br/>Depending on their use, certificates stored in a different format.

The following formats are used:

- Public key only (`.crt`): A file that contains only the public key of
  a certificate with an optional chain of parent certificates (public keys of certificates
  used to signed the certificate).
  <br/>Since this format contains only public keys, it is not a problem if its contents
  are exposed. It must still be store it in a safe place to avoid losing it.
- Private key only (`.key`): A file that contains only the private key of a certificate.
  <br/>It is vital to protect these files and store them in a safe place.
- Keyfile with public & private key (`.keyfile`): A file that contains the public key of
  a certificate, an optional chain of parent certificates and a private key.
  <br/>Since this format also contains a private key, it is vital to protect these files
  and store them in a safe place.
- Java keystore (`.jks`): A file containing a set of public and private keys.
  <br/>It is possible to protect access to the content of this file using a keystore password.
  <br/>Since this format can contain private keys, it is vital to protect these files
  and store them in a safe place (even when its content is protected with a keystore password).

### Creating certificates

ArangoSync provides commands to create all certificates needed.

#### TLS server certificates

To create a certificate used for TLS servers in the **keyfile** format,
you need the public key of the CA (`--cacert`), the private key of
the CA (`--cakey`) and one or more hostnames (or IP addresses).
Then run:

```bash
arangosync create tls keyfile \
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
arangosync create tls certificate \
    --cacert=my-tls-ca.crt --cakey=my-tls-ca.key \
    --host=<hostname> \
    --cert=my-tls-cert.crt \
    --key=my-tls-cert.key \
```

Make sure to protect and store the generated files (`my-tls-cert.crt` & `my-tls-cert.key`) in a safe place.

#### Client authentication certificates

To create a certificate used for client authentication in the **keyfile** format,
you need the public key of the CA (`--cacert`), the private key of
the CA (`--cakey`) and one or more hostnames (or IP addresses) or email addresses.
Then run:

```bash
arangosync create client-auth keyfile \
    --cacert=my-client-auth-ca.crt --cakey=my-client-auth-ca.key \
    [--host=<hostname> | --email=<emailaddress>] \
    --keyfile=my-client-auth-cert.keyfile
```

Make sure to protect and store the generated keyfile (`my-client-auth-cert.keyfile`) in a safe place.

#### CA certificates

To create a CA certificate used to **sign TLS certificates**, run:

```bash
arangosync create tls ca \
    --cert=my-tls-ca.crt --key=my-tls-ca.key 
```

Make sure to protect and store both generated files (`my-tls-ca.crt` & `my-tls-ca.key`) in a safe place.

To create a CA certificate used to **sign client authentication certificates**, run:

```bash
arangosync create client-auth ca \
    --cert=my-client-auth-ca.crt --key=my-client-auth-ca.key
```

Make sure to protect and store both generated files (`my-client-auth-ca.crt` & `my-client-auth-ca.key`)
in a safe place.
<br/>Note: CA certificates have a much longer lifetime than normal certificates.
Therefore even more care is needed to store them safely.

### Renewing certificates

All certificates have meta information in them the limit their use in function,
target & lifetime.
<br/> A certificate created for client authentication (function) cannot be used as a TLS server certificate
(same is true for the reverse).
<br/> A certificate for host `myserver` (target) cannot be used for host `anotherserver`.
<br/> A certificate that is valid until October 2017 (lifetime) cannot be used after October 2017.

If anything changes in function, target or lifetime you need a new certificate.

The procedure for creating a renewed certificate is the same as for creating a "first" certificate.
<br/> After creating the renewed certificate the process(es) using them have to be updated.
This mean restarting them. All ArangoSync components are designed to support stopping and starting
single instances, but do not restart more than 1 instance at the same time.
As soon as 1 instance has been restarted, give it some time to "catch up" before restarting
the next instance.
