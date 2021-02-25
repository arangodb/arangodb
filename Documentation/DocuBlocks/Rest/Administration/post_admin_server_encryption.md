
@startDocuBlock post_admin_server_encryption
@brief Rotate encryption at rest key

@RESTHEADER{POST /_admin/server/encryption, Rotate the encryption at rest keystore, handleEncryption:post}

@RESTDESCRIPTION
Change the user supplied encryption at rest key by sending a request without
payload to this endpoint. The file supplied via `--rocksdb.encryption-keyfolder`
will be reloaded and the internal encryption key will be re-encrypted with the
new user key.

This is a protected API and can only be executed with superuser rights.
This API is not available on coordinator nodes.

The API returns HTTP 404 in case encryption key rotation is disabled.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200 if everything is ok

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*false* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code - 200 in this case

@RESTREPLYBODY{result,object,required,jwt_keys_struct}
The result object.

@RESTSTRUCT{encryption-keys,jwt_keys_struct,array,required,object}
An array of objects with the SHA-256 hashes of the key secrets.
Can be empty.

@RESTRETURNCODE{403}
This API will return HTTP 403 FORBIDDEN if it is not called with
superuser rights.

@RESTRETURNCODE{404}
This API will return HTTP 404 in case encryption key rotation is disabled.
@endDocuBlock
