HTTP Handling in ArangoDB {#Communication}
==========================================

@NAVIGATE_Communication
@EMBEDTOC{CommunicationTOC}

Clients sending requests to ArangoDB must use either HTTP 1.0 or HTTP 1.1.
Other HTTP versions are not supported by ArangoDB and any attempt to send 
a different HTTP version signature will result in the server responding with
an HTTP 505 (HTTP version not supported) error.

ArangoDB will always respond to client requests with HTTP 1.1. Clients
should therefore support HTTP version 1.1. 

The maximum URL length accepted by ArangoDB is 16K. Incoming requests with
longer URLs will be rejected with an HTTP 414 (Request-URI too long) error.

Keep-Alive and Authentication {#CommunicationKeepAlive}
=======================================================

ArangoDB supports HTTP keep-alive. If the client does not send a `Connection`
header in its request, and the client uses HTTP version 1.1, ArangoDB will assume 
the client wants to keep alive the connection. 
If clients do not wish to use the keep-alive feature, they should
explicitly indicate that by sending a `Connection: Close` HTTP header in 
the request.

ArangoDB will close connections automatically for clients that send requests
using HTTP 1.0, except if they send an `Connection: Keep-Alive` header.

The default Keep-Alive timeout can be specified at server start using the
`--server.keep-alive-timeout` parameter.

Client authentication is done by using the `Authorization` HTTP header.
ArangoDB supports Basic authentication.

Authentication is optional. To enforce authentication for incoming requested,
the server must be started with the option `--server.disable-authentication`.
Please note that requests using the HTTP OPTIONS method will be answered by
ArangoDB in any case, even if no authentication data is sent by the client or if
the authentication data is wrong. This is required for handling CORS preflight
requests (see @ref CommunicationCors). The response to an HTTP OPTIONS request
will be generic and not expose any private data.

Please note that when authentication is turned on in ArangoDB, it will by
default affect all incoming requests. Since ArangoDB 1.4, there is an additional
option to restrict authentication to requests to the ArangoDB internal APIs and
the admin interface. 
This option can be used to expose a public API built with ArangoDB to the outside
world without the need for HTTP authentication, but to still protect the usage of the
ArangoDB API (i.e. `/_api/*`) and the admin interface (i.e. `/_admin/*`) with
HTTP authentication.

This behavior can be controlled with the option `--server.authenticate-system-only`
startup parameter. It is set to `false` by default so when using authentication,
all incoming requests need HTTP authentication. Setting the option to `false` will
only require requests to the internal functionality require authentication but 
will allow unauthenticated requests to all other URLs.

Error Handling {#CommunicationErrors}
=====================================

The following should be noted about how ArangoDB handles client errors in its
HTTP layer:

- ArangoDB will reject client requests with a negative value in the
  `Content-Length` request header with `HTTP 411` (Length Required).

- if the client sends a `Content-Length` header with a value bigger than 0 for
  an HTTP GET, HEAD, or DELETE request, ArangoDB will process the request, but
  will write a warning to its log file.

- when the client sends a `Content-Length` header that has a value that is lower
  than the actual size of the body sent, ArangoDB will respond with `HTTP 400`
  (Bad Request).

- if clients send a `Content-Length` value bigger than the actual size of the
  body of the request, ArangoDB will wait for about 90 seconds for the client to
  complete its request. If the client does not send the remaining body data
  within this time, ArangoDB will close the connection. Clients should avoid
  sending such malformed requests as they will make ArangoDB block waiting for
  more data to arrive.

- when clients send a body or a `Content-Length` value bigger than the maximum
  allowed value (512 MB), ArangoDB will respond with `HTTP 413` (Request Entity
  Too Large).

- if the overall length of the HTTP headers a client sends for one request
  exceeds the maximum allowed size (1 MB), the server will fail with `HTTP 431`
  (Request Header Fields Too Large).

- if clients request a HTTP method that is not supported by the server, ArangoDB
  will return with `HTTP 405` (Method Not Allowed). ArangoDB offers general
  support for the following HTTP methods:
  - GET
  - POST
  - PUT
  - DELETE
  - HEAD
  - PATCH
  - OPTIONS
  
  Please note that not all server actions allow using all of these HTTP methods.
  You should look up up the supported methods for each method you intend to use
  in the manual.

  Requests using any other HTTP method (such as for example CONNECT, TRACE etc.)
  will be rejected by ArangoDB.

Cross Origin Resource Sharing (CORS) requests {#CommunicationCors}
==================================================================

ArangoDB will automatically handle CORS requests as follows:

- when the client sends an `Origin` HTTP header, ArangoDB will return a header
  `access-control-allow-origin` containing the value the client sent in the
  `Origin` header.

- for non-trivial CORS requests, clients may issue a preflight request via an
  additional HTTP OPTIONS request.  ArangoDB will automatically answer such
  preflight HTTP OPTIONS requests with an HTTP 200 response with an empty
  body. ArangoDB will return the following headers in the response:

  - `access-control-allow-origin`: will contain the value that the client
    provided in the `Origin` header of the request

  - `access-control-allow-methods`: will contain a list of all HTTP methods
    generally supported by ArangoDB. This list does not depend on the URL the 
    client requested and is the same for all CORS requests. 

  - `access-control-allow-headers`: will contain exactly the value that
    the client has provided in the `Access-Control-Request-Header` header 
    of the request. This header will only be returned if the client has
    specified the header in the request. ArangoDB will send back the original
    value without further validation.

  - `access-control-max-age`: will return a cache lifetime for the preflight
    response as determined by ArangoDB.

- any `access-control-allow-credentials` header sent by the client is ignored by
  ArangoDB its value is not `true`. If a client sends a header value of `true`,
  ArangoDB will return the header `access-control-allow-credentials: true`, too.

Note that CORS preflight requests will probably not send any authentication data
with them. One of the purposes of the preflight request is to check whether the
server accepts authentication or not.

A consequence of this is that ArangoDB will allow requests using the HTTP
OPTIONS method without credentials, even when the server is run with
authentication enabled.

The response to the HTTP OPTIONS request will however be a generic response that
will not expose any private data and thus can be considered "safe" even without
credentials.

