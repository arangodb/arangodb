
@startDocuBlock post_admin_server_encryption
@brief Rotate encryption at rest key

@RESTHEADER{POST /_admin/server/encryption, Rotate the encryption at rest key, handleEncryption:post}

@RESTDESCRIPTION
Change the user supplied encryption at rest key by sending a request without
payload to this endpoint. The file supplied via `--rocksdb.encryption-keyfile`
will be reloaded and the internal encryption key will be re-encrypted with the
new user key.

This is a protected API and can only be executed with superuser rights.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200 if everything is ok

@RESTRETURNCODE{403}
This API will return HTTP 403 FORBIDDEN if it is not called with
superuser rights.
@endDocuBlock
