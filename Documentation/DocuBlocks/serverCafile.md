

@brief CA file
`--ssl.cafile filename`

This option can be used to specify a file with CA certificates that are
sent
to the client whenever the server requests a client certificate. If the
file is specified, The server will only accept client requests with
certificates issued by these CAs. Do not specify this option if you want
clients to be able to connect without specific certificates.

The certificates in *filename* must be PEM formatted.

**Note**: this option is only relevant if at least one SSL endpoint is
used.

