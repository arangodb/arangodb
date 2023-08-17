
@startDocuBlock post_admin_server_jwt

@RESTHEADER{POST /_admin/server/jwt, Hot-reload the JWT secret(s) from disk, reloadServerJwtSecrets}

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

@RESTREPLYBODY{,object,required,admin_server_jwt}
The reply with the JWT secrets information.

@RESTRETURNCODE{403}
if the request was not authenticated as a user with sufficient rights

@endDocuBlock
