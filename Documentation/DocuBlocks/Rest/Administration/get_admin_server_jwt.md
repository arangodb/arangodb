
@startDocuBlock get_admin_server_jwt
@brief Retrieve JWT secrets info

@RESTHEADER{GET /_admin/server/jwt, Fetch information about the currently loaded secrets, handleJWT:get}

@RESTDESCRIPTION
Get information about the currently loaded secrets.

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
