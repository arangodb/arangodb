
@startDocuBlock get_admin_server_tls
@brief Return the TLS data of this server (server key, client-auth CA)

@RESTHEADER{GET /_admin/server/tls, Return a summary of the TLS data, handleMode:get}

@RESTDESCRIPTION
Return a summary of the TLS data. The JSON response will contain a field
`result` with the following components:

  - `keyfile`: Information about the key file.
  - `clientCA`: Information about the CA for client certificate
    verification.

In both cases the value of the attribute will be a JSON object, which
has a subset of the following attributes (whatever is appropriate):

  - `SHA256`: The value is a string with the SHA256 of the whole input
    file.
  - `certificates`: The value is a JSON array with the public
    certificates in the chain in the file.
  - `privateKeySHA256`: In cases where there is a private key (`keyfile`
    but not `clientCA`), this field is present and contains a
    JSON string with the SHA256 of the private key.

This is a public API so it does *not* require authentication.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200 if everything is ok
@endDocuBlock
