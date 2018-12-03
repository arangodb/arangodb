# ArangoDB Server SSL Options

## SSL Endpoints

Given a hostname:

`--server.endpoint ssl://hostname:port`

Given an IPv4 address:

`--server.endpoint ssl://ipv4-address:port`

Given an IPv6 address:

`--server.endpoint ssl://[ipv6-address]:port`

**Note**: If you are using SSL-encrypted endpoints, you must also supply the
path to a server certificate using the `--ssl.keyfile` option.

### Keyfile

`--ssl.keyfile filename`

If SSL encryption is used, this option must be used to specify the filename of
the server private key. The file must be PEM formatted and contain both the
certificate and the server's private key.

The file specified by *filename* can be generated using OpenSSL:

```
# create private key in file "server.key"
openssl genpkey -out server.key -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -aes-128-cbc

# create certificate signing request (csr) in file "server.csr"
openssl req -new -key server.key -out server.csr

# copy away original private key to "server.key.org"
cp server.key server.key.org

# remove passphrase from the private key
openssl rsa -in server.key.org -out server.key

# sign the csr with the key, creates certificate PEM file "server.crt"
openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

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

For further information please check the manuals of the tools you use to create
the certificate.

### CA File

`--ssl.cafile filename`

This option can be used to specify a file with CA certificates that are sent to
the client whenever the server requests a client certificate. If the file is
specified, The server will only accept client requests with certificates issued
by these CAs. Do not specify this option if you want clients to be able to
connect without specific certificates.

The certificates in *filename* must be PEM formatted.

### SSL protocol

`--ssl.protocol value`

Use this option to specify the default encryption protocol to be used.  The
following variants are available:

- 1: SSLv2 (unsupported)
- 2: SSLv2 or SSLv3 (negotiated)
- 3: SSLv3
- 4: TLSv1
- 5: TLSv1.2

The default *value* is 5 (TLSv1.2).

Note that SSLv2 is unsupported as of ArangoDB 3.4, because of the inherent 
security vulnerabilities in this protocol. Selecting SSLv2 as protocol will
abort the startup.

### SSL cache

`--ssl.session-cache value`

Set to true if SSL session caching should be used.

*value* has a default value of *false* (i.e. no caching).

### SSL peer certificate

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

`--ssl.require-peer-certificate`

Require a peer certificate from the client before connecting.

### SSL options

`--ssl.options value`

This option can be used to set various SSL-related options. Individual option
values must be combined using bitwise OR.

Which options are available on your platform is determined by the OpenSSL
version you use. The list of options available on your platform might be
retrieved by the following shell command:

```
 > grep "#define SSL_OP_.*" /usr/include/openssl/ssl.h

 #define SSL_OP_MICROSOFT_SESS_ID_BUG                    0x00000001L
 #define SSL_OP_NETSCAPE_CHALLENGE_BUG                   0x00000002L
 #define SSL_OP_LEGACY_SERVER_CONNECT                    0x00000004L
 #define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG         0x00000008L
 #define SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG              0x00000010L
 #define SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER               0x00000020L
 ...
```

A description of the options can be found online in the
[OpenSSL documentation](http://www.openssl.org/docs/ssl/SSL_CTX_set_options.html)

### SSL cipher

`--ssl.cipher-list cipher-list`

This option can be used to restrict the server to certain SSL ciphers only, and
to define the relative usage preference of SSL ciphers.

The format of *cipher-list* is documented in the OpenSSL documentation.

To check which ciphers are available on your platform, you may use the
following shell command:

```
> openssl ciphers -v

ECDHE-RSA-AES256-SHA    SSLv3 Kx=ECDH     Au=RSA  Enc=AES(256)  Mac=SHA1
ECDHE-ECDSA-AES256-SHA  SSLv3 Kx=ECDH     Au=ECDSA Enc=AES(256)  Mac=SHA1
DHE-RSA-AES256-SHA      SSLv3 Kx=DH       Au=RSA  Enc=AES(256)  Mac=SHA1
DHE-DSS-AES256-SHA      SSLv3 Kx=DH       Au=DSS  Enc=AES(256)  Mac=SHA1
DHE-RSA-CAMELLIA256-SHA SSLv3 Kx=DH       Au=RSA  Enc=Camellia(256)
Mac=SHA1
...
```

The default value for *cipher-list* is "ALL".
