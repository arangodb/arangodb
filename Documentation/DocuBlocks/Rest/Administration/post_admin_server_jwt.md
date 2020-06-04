
@startDocuBlock post_admin_server_jwt
@brief Hot-reload JWT secrets

@RESTHEADER{POST /_admin/server/jwt, Hot-reload the JWT secret(s) from disk, handleJWT:post}

@RESTDESCRIPTION
Sending a request without payload to this endpoint reloads the JWT secret(s)
from disk. Only the files specified via the arangod startup option
`--server.jwt-secret-keyfile` or `--server.jwt-secret-folder` are used.
It is not possible to change the locations where files are loaded from
without restarting the process.

To utilize the API a superuser JWT token is necessary, otherwise the response
will be _HTTP 403 Forbidden_.

@RESTRETURNCODES

@RESTRETURNCODE{200}

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*false* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code - 200 in this case

@RESTREPLYBODY{result,object,required,jwt_secret_struct}
The result object.

@RESTSTRUCT{active,jwt_secret_struct,object,required,}
An object with the SHA-256 hash of the active secret.

@RESTSTRUCT{passive,jwt_secret_struct,array,required,object}
An array of objects with the SHA-256 hashes of the passive secrets.
Can be empty.

@RESTRETURNCODE{403}
if the request was not authenticated as a user with sufficient rights

@endDocuBlock
