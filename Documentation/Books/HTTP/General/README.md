General HTTP Request Handling in ArangoDB
=========================================

Protocol
--------

ArangoDB exposes its API via HTTP, making the server accessible easily with
a variety of clients and tools (e.g. browsers, curl, telnet). The communication
can optionally be SSL-encrypted.

ArangoDB uses the standard HTTP methods (e.g. *GET*, *POST*, *PUT*, *DELETE*) plus
the *PATCH* method described in [RFC 5789](http://tools.ietf.org/html/rfc5789).

Most server APIs expect clients to send any payload data in [JSON](http://www.json.org)
format. Details on the expected format and JSON attributes can be found in the
documentation of the individual server methods.

Clients sending requests to ArangoDB must use either HTTP 1.0 or HTTP 1.1.
Other HTTP versions are not supported by ArangoDB and any attempt to send
a different HTTP version signature will result in the server responding with
an HTTP 505 (HTTP version not supported) error.

ArangoDB will always respond to client requests with HTTP 1.1. Clients
should therefore support HTTP version 1.1.

Clients are required to include the *Content-Length* HTTP header with the
correct content length in every request that can have a body (e.g. *POST*,
*PUT* or *PATCH*) request. ArangoDB will not process requests without a
*Content-Length* header - thus chunked transfer encoding for POST-documents
is not supported.

HTTP Keep-Alive
---------------

ArangoDB supports HTTP keep-alive. If the client does not send a *Connection*
header in its request, and the client uses HTTP version 1.1, ArangoDB will assume
the client wants to keep alive the connection.
If clients do not wish to use the keep-alive feature, they should
explicitly indicate that by sending a *Connection: Close* HTTP header in
the request.

ArangoDB will close connections automatically for clients that send requests
using HTTP 1.0, except if they send an *Connection: Keep-Alive* header.

The default Keep-Alive timeout can be specified at server start using the
*--http.keep-alive-timeout* parameter.

Establishing TCP connections is expensive, since it takes several ping pongs
between the communication parties. Therefore you can use connection keepalive
to send several HTTP request over one TCP-connection;
Each request is treated independently by definition. You can use this feature
to build up a so called *connection pool* with several established
connections in your client application, and dynamically re-use
one of those then idle connections for subsequent requests.

Blocking vs. Non-blocking HTTP Requests
---------------------------------------

ArangoDB supports both blocking and non-blocking HTTP requests.

ArangoDB is a multi-threaded server, allowing the processing of multiple
client requests at the same time. Request/response handling and the actual
work are performed on the server in parallel by multiple worker threads.

Still, clients need to wait for their requests to be processed by the server,
and thus keep one connection of a pool occupied.
By default, the server will fully process an incoming request and then return
the result to the client when the operation is finished. The client must
wait for the server's HTTP response before it can send additional requests over
the same connection. For clients that are single-threaded and/or are
blocking on I/O themselves, waiting idle for the server response may be
non-optimal.

To reduce blocking on the client side, ArangoDB offers a generic mechanism for
non-blocking, asynchronous execution: clients can add the
HTTP header *x-arango-async: true* to any of their requests, marking
them as to be executed asynchronously on the server. ArangoDB will put such
requests into an in-memory task queue and return an *HTTP 202* (accepted)
response to the client instantly and thus finish this HTTP-request.
The server will execute the tasks from the queue asynchronously as fast
as possible, while clients can continue to do other work.
If the server queue is full (i.e. contains as many tasks as specified by the
option ["--server.maximal-queue-size"](../../Manual/Programs/Arangod/Options.html#server-options)),
then the request will be rejected instantly with an *HTTP 500* (internal
server error) response.

Asynchronous execution decouples the request/response handling from the actual
work to be performed, allowing fast server responses and greatly reducing wait
time for clients. Overall this allows for much higher throughput than if
clients would always wait for the server's response.

Keep in mind that the asynchronous execution is just "fire and forget".
Clients will get any of their asynchronous requests answered with a generic
HTTP 202 response. At the time the server sends this response, it does not
know whether the requested operation can be carried out successfully (the
actual operation execution will happen at some later point). Clients therefore
cannot make a decision based on the server response and must rely on their
requests being valid and processable by the server.

Additionally, the server's asynchronous task queue is an in-memory data
structure, meaning not-yet processed tasks from the queue might be lost in
case of a crash. Clients should therefore not use the asynchronous feature
when they have strict durability requirements or if they rely on the immediate
result of the request they send.

For details on the subsequent processing
[read on under Async Result handling](../AsyncResultsManagement/README.md).

Authentication
--------------

Client authentication can be achieved by using the *Authorization* HTTP header in
client requests. ArangoDB supports authentication via HTTP Basic or JWT.

Authentication is turned on by default for all internal database APIs but turned off for custom Foxx apps.
To toggle authentication for incoming requests to the internal database APIs, use the option
[--server.authentication](../../Manual/Programs/Arangod/Server.html#enabledisable-authentication).
This option is turned on by default so authentication is required for the database APIs.

Please note that requests using the HTTP OPTIONS method will be answered by
ArangoDB in any case, even if no authentication data is sent by the client or if
the authentication data is wrong. This is required for handling CORS preflight
requests (see [Cross Origin Resource Sharing requests](#cross-origin-resource-sharing-cors-requests)).
The response to an HTTP OPTIONS request will be generic and not expose any private data.

There is an additional option to control authentication for custom Foxx apps. The option
[--server.authentication-system-only](../../Manual/Programs/Arangod/Server.html#enabledisable-authentication-for-system-api-requests-only)
controls whether authentication is required only for requests to the internal database APIs and the admin interface.
It is turned on by default, meaning that other APIs (this includes custom Foxx apps) do not require authentication.

The default values allow exposing a public custom Foxx API built with ArangoDB to the outside
world without the need for HTTP authentication, but still protecting the usage of the
internal database APIs (i.e. */_api/*, */_admin/*) with HTTP authentication.

If the server is started with the *--server.authentication-system-only* option set
to *false*, all incoming requests will need HTTP authentication if the server is configured
to require HTTP authentication (i.e. *--server.authentication true*).
Setting the option to *true* will make the server require authentication only for requests to the
internal database APIs and will allow unauthenticated requests to all other URLs.

Here's a short summary:

* `--server.authentication true --server.authentication-system-only true`: this will require
  authentication for all requests to the internal database APIs but not custom Foxx apps.
  This is the default setting.
* `--server.authentication true --server.authentication-system-only false`: this will require
  authentication for all requests (including custom Foxx apps).
* `--server.authentication false`: authentication disabled for all requests

Whenever authentication is required and the client has not yet authenticated,
ArangoDB will return *HTTP 401* (Unauthorized). It will also send the *WWW-Authenticate*
response header, indicating that the client should prompt the user for username and
password if supported. If the client is a browser, then sending back this header will
normally trigger the display of the browser-side HTTP authentication dialog.
As showing the browser HTTP authentication dialog is undesired in AJAX requests,
ArangoDB can be told to not send the *WWW-Authenticate* header back to the client.
Whenever a client sends the *X-Omit-WWW-Authenticate* HTTP header (with an arbitrary value)
to ArangoDB, ArangoDB will only send status code 401, but no *WWW-Authenticate* header.
This allows clients to implement credentials handling and bypassing the browser's
built-in dialog.

### Authentication via JWT

ArangoDB uses a standard JWT based authentication method. 
To authenticate via JWT you must first obtain a JWT token with a signature generated via HMAC with SHA-256. 
The secret may either be set using `--server.jwt-secret` or will be randomly generated upon server startup.

For more information on JWT please consult RFC7519 and https://jwt.io

#### User JWT-Token

To authenticate with a specific user you need to supply a JWT token containing
the _preferred_username_ field with the username. 
You can either let ArangoDB generate this token for you via an API call
or you can generate it yourself (only if you know the JWT secret).

ArangoDB offers a REST API to generate user tokens for you if you know the username and password. 
To do so send a POST request to

*/_open/auth*
containing *username* and *password* JSON-encoded like so:

{"username":"root","password":"rootPassword"}

Upon success the endpoint will return a **200 OK** and an answer containing the JWT in a JSON-
encoded object like so:

```
{"jwt":"eyJhbGciOiJIUzI1NiI..x6EfI"}
```

This JWT should then be used within the Authorization HTTP header in subsequent requests:

```
Authorization: bearer eyJhbGciOiJIUzI1NiI..x6EfI
```

Please note that the JWT will expire after 1 month and needs to be updated. We encode the expiration
date of the JWT token in the _exp_ field in unix time.
Please note that all JWT tokens must contain the _iss_ field with string value `arangodb`.
As an example the decoded JWT body would look like this:

```json
{
  "exp": 1540381557,
  "iat": 1537789.55727901,
  "iss": "arangodb",
  "preferred_username": "root"
}
```

#### Superuser JWT-Token

To access specific internal APIs as well as Agency and DBServer instances a token generated via `/open/auth` is not 
good enough. For these special APIs you will need to generate a special JWT token which grants superuser access.
Note that using superuser access for normal database operations is **NOT advised**.

_Note_: It is only possible to generate this JWT token with the knowledge of the JWT secret.

For your convenience it is possible to generate this token via the [ArangoDB starter CLI](../../Manual/Programs/Starter/Security.html#using-authentication-tokens).

Should you wish to generate the JWT token yourself with a tool of your choice, you need to include the correct body.
The body must contain the _iss_ field with string value `arangodb` and the `server_id` field with an arbitrary string identifier:

```json
{
  "exp": 1537900279,
  "iat": 1537800279,
  "iss": "arangodb",
  "server_id": "myclient"
}
```

For example to generate a token via the [jwtgen tool](https://www.npmjs.com/package/jwtgen) (note the lifetime of one hour):

```
jwtgen -s <my-secret> -e 3600 -v -a "HS256" -c 'iss=arangodb' -c 'server_id=myclient'
curl -v -H "Authorization: bearer $(jwtgen -s <my-secret> -e 3600 -a "HS256" -c 'iss=arangodb' -c 'server_id=myclient')" http://<database-ip>:8529/_api/version
```

Error Handling
--------------

The following should be noted about how ArangoDB handles client errors in its
HTTP layer:

* client requests using an HTTP version signature different than *HTTP/1.0* or
  *HTTP/1.1* will get an *HTTP 505* (HTTP version not supported) error in return.
* ArangoDB will reject client requests with a negative value in the
  *Content-Length* request header with *HTTP 411* (Length Required).
* ArangoDB doesn't support POST with *transfer-encoding: chunked* which forbids
  the *Content-Length* header above.
* the maximum URL length accepted by ArangoDB is 16K. Incoming requests with
  longer URLs will be rejected with an *HTTP 414* (Request-URI too long) error.
* if the client sends a *Content-Length* header with a value bigger than 0 for
  an HTTP GET, HEAD, or DELETE request, ArangoDB will process the request, but
  will write a warning to its log file.
* when the client sends a *Content-Length* header that has a value that is lower
  than the actual size of the body sent, ArangoDB will respond with *HTTP 400*
  (Bad Request).
* if clients send a *Content-Length* value bigger than the actual size of the
  body of the request, ArangoDB will wait for about 90 seconds for the client to
  complete its request. If the client does not send the remaining body data
  within this time, ArangoDB will close the connection. Clients should avoid
  sending such malformed requests as this will block one tcp connection,
  and may lead to a temporary file descriptor leak.
* when clients send a body or a *Content-Length* value bigger than the maximum
  allowed value (512 MB), ArangoDB will respond with *HTTP 413* (Request Entity
  Too Large).
* if the overall length of the HTTP headers a client sends for one request
  exceeds the maximum allowed size (1 MB), the server will fail with *HTTP 431*
  (Request Header Fields Too Large).
* if clients request an HTTP method that is not supported by the server, ArangoDB
  will return with *HTTP 405* (Method Not Allowed). ArangoDB offers general
  support for the following HTTP methods:
  * GET
  * POST
  * PUT
  * DELETE
  * HEAD
  * PATCH
  * OPTIONS

  Please note that not all server actions allow using all of these HTTP methods.
  You should look up the supported methods for each method you intend to use
  in the manual.

  Requests using any other HTTP method (such as for example CONNECT, TRACE etc.)
  will be rejected by ArangoDB as mentioned before.

Cross-Origin Resource Sharing (CORS) requests
---------------------------------------------

ArangoDB will automatically handle CORS requests as follows:

### Preflight

When a browser is told to make a cross-origin request that includes explicit
headers, credentials or uses HTTP methods other than `GET` or `POST`, it will
first perform a so-called preflight request using the `OPTIONS` method.

ArangoDB will respond to `OPTIONS` requests with an HTTP 200 status response
with an empty body. Since preflight requests are not expected to include or
even indicate the presence of authentication credentials even when they will
be present in the actual request, ArangoDB does not enforce authentication for
`OPTIONS` requests even when authentication is enabled.

ArangoDB will set the following headers in the response:

* `access-control-allow-credentials`: will be set to `false` by default.
  For details on when it will be set to `true` see the next section on cookies.

* `access-control-allow-headers`: will be set to the exact value of the
  request's `access-control-request-headers` header or omitted if no such
  header was sent in the request.

* `access-control-allow-methods`: will be set to a list of all supported HTTP
  headers regardless of the target endpoint. In other words that a method is
  listed in this header does not guarantee that it will be supported by the
  endpoint in the actual request.

* `access-control-allow-origin`: will be set to the exact value of the
  request's `origin` header.

* `access-control-expose-headers`: will be set to a list of response headers used
  by the ArangoDB HTTP API.

* `access-control-max-age`: will be set to an implementation-specific value.

### Actual request

If a request using any other HTTP method than `OPTIONS` includes an `origin` header,
ArangoDB will add the following headers to the response:

* `access-control-allow-credentials`: will be set to `false` by default.
  For details on when it will be set to `true` see the next section on cookies.

* `access-control-allow-origin`: will be set to the exact value of the
  request's `origin` header.

* `access-control-expose-headers`: will be set to a list of response headers used
  by the ArangoDB HTTP API.

When making CORS requests to endpoints of Foxx services, the value of the
`access-control-expose-headers` header will instead be set to a list of
response headers used in the response itself (but not including the
`access-control-` headers). Note that [Foxx services may override this behavior](../../Manual/Foxx/Guides/Browser.html#cross-origin-resource-sharing-cors).

### Cookies and authentication

In order for the client to be allowed to correctly provide authentication
credentials or handle cookies, ArangoDB needs to set the
`access-control-allow-credentials` response header to `true` instead of `false`.

ArangoDB will automatically set this header to `true` if the value of the
request's `origin` header matches a trusted origin in the `http.trusted-origin`
configuration option. To make ArangoDB trust a certain origin, you can provide
a startup option when running `arangod` like this:

`--http.trusted-origin "http://localhost:8529"`

To specify multiple trusted origins, the option can be specified multiple times.
Alternatively you can use the special value `"*"` to trust any origin:

`--http.trusted-origin "*"`

Note that browsers will not actually include credentials or cookies in cross-origin
requests unless explicitly told to do so:

* When using the Fetch API you need to set the
  [`credentials` option to `include`](https://fetch.spec.whatwg.org/#cors-protocol-and-credentials).

  ```js
  fetch("./", { credentials:"include" }).then(/* … */)
  ```

* When using `XMLHttpRequest` you need to set the
  [`withCredentials` option to `true`](https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest/withCredentials).

  ```js
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'https://example.com/', true);
  xhr.withCredentials = true;
  xhr.send(null);
  ```

* When using jQuery you need to set the `xhrFields` option:

  ```js
  $.ajax({
     url: 'https://example.com',
     xhrFields: {
        withCredentials: true
     }
  });
  ```

HTTP method overriding
----------------------

ArangoDB provides a startup option *--http.allow-method-override*.
This option can be set to allow overriding the HTTP request method (e.g. GET, POST,
PUT, DELETE, PATCH) of a request using one of the following custom HTTP headers:

* *x-http-method-override*
* *x-http-method*
* *x-method-override*

This allows using HTTP clients that do not support all "common" HTTP methods such as
PUT, PATCH and DELETE. It also allows bypassing proxies and tools that would otherwise
just let certain types of requests (e.g. GET and POST) pass through.

Enabling this option may impose a security risk, so it should only be used in very
controlled environments. Thus the default value for this option is *false* (no method
overriding allowed). You need to enable it explicitly if you want to use this
feature.

Load-balancer support
---------------------

When running in cluster mode, ArangoDB exposes some APIs which store request
state data on specific coordinator nodes, and thus subsequent requests which
require access to this state must be served by the coordinator node which owns
this state data. In order to support function behind a load-balancer, ArangoDB
can transparently forward requests within the cluster to the correct node. If a
request is forwarded, the response will contain the following custom HTTP header
whose value will be the ID of the node which actually answered the request:

* *x-arango-request-forwarded-to*

The following APIs may use request forwarding:

* `/_api/cursor`
* `/_api/job`
* `/_api/tasks`

Note: since forwarding such requests require an additional cluster-internal HTTP
request, they should be avoided when possible for best performance. Typically
this is accomplished either by directing the requests to the correct coordinator
at a client-level or by enabling request "stickiness" on a load balancer. Since
these approaches are not always possible in a given environment, we support the
request forwarding as a fall-back solution.

Note: some endpoints which return "global" data, such as `GET /_api/tasks` will
only return data corresponding to the server on which the request is executed.
These endpoints will generally not work well with load-balancers.
