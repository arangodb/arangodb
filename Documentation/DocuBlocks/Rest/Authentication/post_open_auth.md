@startDocuBlock post_open_auth

@RESTHEADER{POST /_open/auth, Create a JWT session token, createSessionToken}

@RESTDESCRIPTION
Obtain a JSON Web Token (JWT) from the credentials of an ArangoDB user account.
You can use the JWT in the `Authorization` HTTP header as a `Bearer` token to
authenticate requests.

The lifetime for the token is controlled by the `--server.session-timeout`
startup option.

@RESTBODYPARAM{username,string,required,}
The name of an ArangoDB user.

@RESTBODYPARAM{password,string,required,}
The password of the ArangoDB user.

@RESTRETURNCODES

@RESTRETURNCODE{200}

@RESTREPLYBODY{jwt,string,required,}
The encoded JWT session token.

@RESTRETURNCODE{400}
An HTTP `400 Bad Request` status code is returned if the request misses required
attributes or if it is otherwise malformed.

@RESTRETURNCODE{401}
An HTTP `401 Unauthorized` status code is returned if the user credentials are
incorrect.

@RESTRETURNCODE{404}
An HTTP `404 Not Found` status code is returned if the server has authentication
disabled and the endpoint is thus not available.

@endDocuBlock
