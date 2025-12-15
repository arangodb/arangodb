#!/bin/bash

# Create CA keypair:

openssl ecparam -name prime256v1 -genkey -noout -out tls-ca.key
openssl req -x509 -new -nodes -extensions v3_ca -key tls-ca.key -days 1024 -out tls-ca.crt -sha512 -subj "/O=ArangoDB/CN=ArangoDB/"

# Produce the key pair signed by the `tls-ca.key`:

openssl ecparam -name prime256v1 -genkey -noout -out key.pem
cat > ssl.conf <<EOF
[req]
prompt = no
distinguished_name = myself

[myself]
O = ArangoDB
CN = ArangoDB

[req_ext]
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = hans.arango.ai
DNS.3 = 127.0.0.1
EOF

openssl req -new -key key.pem -out key-csr.pem -sha512 -config ssl.conf -subj "/O=ArangoDB/CN=localhost"

openssl x509 -req -in key-csr.pem -CA tls-ca.crt -days 3650 -CAkey tls-ca.key -out cert.pem -extensions req_ext -extfile ssl.conf -CAcreateserial

# Put server certificate, CA certificate and server key into keyfile.
# Note that the order is important here: The server cert must go before
# the CA cert!
cat cert.pem tls-ca.crt key.pem > tls.keyfile
