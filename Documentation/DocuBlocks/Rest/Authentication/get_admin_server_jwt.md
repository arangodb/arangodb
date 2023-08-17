@startDocuBlock get_admin_server_jwt

@RESTHEADER{GET /_admin/server/jwt, Get information about the loaded JWT secrets, getServerJwtSecrets}

@RESTDESCRIPTION
Get information about the currently loaded secrets.

To utilize the API a superuser JWT token is necessary, otherwise the response
will be _HTTP 403 Forbidden_.

@RESTRETURNCODES

@RESTRETURNCODE{200}

@RESTREPLYBODY{,object,required,admin_server_jwt}
The reply with the JWT secrets information.

@RESTRETURNCODE{403}
if the request was not authenticated as a user with sufficient rights

@endDocuBlock
