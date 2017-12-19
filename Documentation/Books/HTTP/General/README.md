!CHAPTER General HTTP Request Handling in ArangoDB 

!SECTION Protocol

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

!SECTION HTTP Keep-Alive

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

!SECTION Blocking vs. Non-blocking HTTP Requests

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
option ["--scheduler.maximal-queue-size"](../../Manual/Administration/Configuration/Communication.html)),
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

!SECTION Authentication

Client authentication can be achieved by using the *Authorization* HTTP header in
client requests. ArangoDB supports HTTP Basic authentication.

Authentication is turned on by default, but can be turned off. To enforce
authentication for incoming requested, the server must be started with the option
[--server.authentication](../../Manual/Administration/Configuration/Arangod.html).
Please note that requests using the HTTP OPTIONS method will be answered by
ArangoDB in any case, even if no authentication data is sent by the client or if
the authentication data is wrong. This is required for handling CORS preflight
requests (see [Cross Origin Resource Sharing requests](#cross-origin-resource-sharing-cors-requests)).
The response to an HTTP OPTIONS request will be generic and not expose any private data.

Please note that when authentication is turned on in ArangoDB, it will by
default affect all incoming requests. 

There is an additional option [--server.authentication-system-only](../../Manual/Administration/Configuration/Arangod.html)
to restrict authentication to requests to the ArangoDB internal APIs and the admin interface. 
This option can be used to expose a public API built with ArangoDB to the outside
world without the need for HTTP authentication, but to still protect the usage of the
ArangoDB API (i.e. */_api/*) and the admin interface (i.e. */_admin/*) with
HTTP authentication.

If the server is started with the *--server.authentication-system-only* parameter set 
to *false* (which is the default), all incoming requests need HTTP authentication if the
server is configured to require HTTP authentication. Setting the option to *false* will
make the server require authentication only for requests to the internal functionality at
*/_api/* or */_admin* and will allow unauthenticated requests to all other URLs.

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

!SECTION Error Handling 

The following should be noted about how ArangoDB handles client errors in its
HTTP layer:

* client requests using an HTTP version signature different than *HTTP/1.0* or 
  *HTTP/1.1* will get an *HTTP 505* (HTTP version not supported) error in return.
* ArangoDB will reject client requests with a negative value in the
  *Content-Length* request header with *HTTP 411* (Length Required).
* Arangodb doesn't support POST with *transfer-encoding: chunked* which forbids
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
  and may lead to a temporary filedescriptor leak.
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
  You should look up up the supported methods for each method you intend to use
  in the manual.

  Requests using any other HTTP method (such as for example CONNECT, TRACE etc.)
  will be rejected by ArangoDB as mentioned before.

!SECTION Cross Origin Resource Sharing (CORS) requests

ArangoDB will automatically handle CORS requests as follows:

* when the client sends an *Origin* HTTP header, ArangoDB will return a header
  *access-control-allow-origin* containing the value the client sent in the
  *Origin* header.
* for non-trivial CORS requests, clients may issue a preflight request via an
  additional HTTP OPTIONS request.  ArangoDB will automatically answer such
  preflight HTTP OPTIONS requests with an HTTP 200 response with an empty
  body. ArangoDB will return the following headers in the response:
  * *access-control-allow-origin*: will contain the value that the client
    provided in the *Origin* header of the request
  * *access-control-allow-methods*: will contain an array of all HTTP methods
    generally supported by ArangoDB. This array does not depend on the URL the 
    client requested and is the same for all CORS requests. 
  * *access-control-allow-headers*: will contain exactly the value that
    the client has provided in the *Access-Control-Request-Header* header 
    of the request. This header will only be returned if the client has
    specified the header in the request. ArangoDB will send back the original
    value without further validation.
  * *access-control-max-age*: will return a cache lifetime for the preflight
    response as determined by ArangoDB.
* any *access-control-allow-credentials* header sent by the client is ignored by
  ArangoDB if its value is not *true*. If a client sends a header value of *true*,
  ArangoDB will return the header *access-control-allow-credentials: true* too,
  but only if the value of the sent `Origin` header matches a trusted origin
  in the `--http.trusted-origin` startup option. To make ArangoDB trust a certain
  origin, specify the origin at server start like this: 
  `--http.trusted-origin "http://localhost:8529"`
  To specify multiple trusted origins, the option can be specified multiple times.
  To trust any origin, the special value `*` can be specified as a trusted origin:
  `--http.trusted-origin "*"`

Note that CORS preflight requests will probably not send any authentication data
with them. One of the purposes of the preflight request is to check whether the
server accepts authentication or not.

A consequence of this is that ArangoDB will allow requests using the HTTP
OPTIONS method without credentials, even when the server is run with
authentication enabled.

The response to the HTTP OPTIONS request will however be a generic response that
will not expose any private data and thus can be considered "safe" even without
credentials.

!SECTION HTTP method overriding

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
