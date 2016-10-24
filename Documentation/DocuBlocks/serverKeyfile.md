

@brief keyfile containing server certificate
`--ssl.keyfile filename`

If SSL encryption is used, this option must be used to specify the filename
of the server private key. The file must be PEM formatted and contain both
the certificate and the server's private key.

The file specified by *filename* should have the following structure:

```
# create private key in file "server.key"
openssl genrsa -des3 -out server.key 1024

# create certificate signing request (csr) in file "server.csr"
openssl req -new -key server.key -out server.csr

# copy away original private key to "server.key.org"
cp server.key server.key.org

# remove passphrase from the private key
openssl rsa -in server.key.org -out server.key

# sign the csr with the key, creates certificate PEM file "server.crt"
openssl x509 -req -days 365 -in server.csr -signkey server.key -out \
server.crt

# combine certificate and key into single PEM file "server.pem"
cat server.crt server.key > server.pem
```

You may use certificates issued by a Certificate Authority or self-signed
certificates. Self-signed certificates can be created by a tool of your
choice. When using OpenSSL for creating the self-signed certificate, the
following commands should create a valid keyfile:

```
-----BEGIN CERTIFICATE-----

(base64 encoded certificate)

-----END CERTIFICATE-----
-----BEGIN RSA PRIVATE KEY-----

(base64 encoded private key)

-----END RSA PRIVATE KEY-----
```

For further information please check the manuals of the tools you use to
create the certificate.

**Note**: the `--ssl.keyfile` option must be set if the server is
started with at least one SSL endpoint.

