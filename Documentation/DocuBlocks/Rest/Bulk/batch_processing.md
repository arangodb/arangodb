
@startDocuBlock batch_processing
@brief executes a batch request

@RESTHEADER{POST /_api/batch,executes a batch request, RestBatchHandler} /// TODOSWAGGER: contentype

@RESTALLBODYPARAM{body,string,required}
The multipart batch request, consisting of the envelope and the individual
batch parts.

@RESTDESCRIPTION
Executes a batch request. A batch request can contain any number of
other requests that can be sent to ArangoDB in isolation. The benefit of
using batch requests is that batching requests requires less client/server
roundtrips than when sending isolated requests.

All parts of a batch request are executed serially on the server. The
server will return the results of all parts in a single response when all
parts are finished.

Technically, a batch request is a multipart HTTP request, with
content-type `multipart/form-data`. A batch request consists of an
envelope and the individual batch part actions. Batch part actions
are "regular" HTTP requests, including full header and an optional body.
Multiple batch parts are separated by a boundary identifier. The
boundary identifier is declared in the batch envelope. The MIME content-type
for each individual batch part must be `application/x-arango-batchpart`.

Please note that when constructing the individual batch parts, you must
use CRLF (`\r\n`) as the line terminator as in regular HTTP messages.

The response sent by the server will be an `HTTP 200` response, with an
optional error summary header `x-arango-errors`. This header contains the
number of batch part operations that failed with an HTTP error code of at
least 400. This header is only present in the response if the number of
errors is greater than zero.

The response sent by the server is a multipart response, too. It contains
the individual HTTP responses for all batch parts, including the full HTTP
result header (with status code and other potential headers) and an
optional result body. The individual batch parts in the result are
seperated using the same boundary value as specified in the request.

The order of batch parts in the response will be the same as in the
original client request. Client can additionally use the `Content-Id`
MIME header in a batch part to define an individual id for each batch part.
The server will return this id is the batch part responses, too.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the batch was received successfully. HTTP 200 is returned
even if one or multiple batch part actions failed.

@RESTRETURNCODE{400}
is returned if the batch envelope is malformed or incorrectly formatted.
This code will also be returned if the content-type of the overall batch
request or the individual MIME parts is not as expected.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@EXAMPLES

Sending a batch request with five batch parts:

- GET /_api/version

- DELETE /_api/collection/products

- POST /_api/collection/products

- GET /_api/collection/products/figures

- DELETE /_api/collection/products

The boundary (`SomeBoundaryValue`) is passed to the server in the HTTP
`Content-Type` HTTP header.
*Please note the reply is not displayed all accurate.*

@EXAMPLE_ARANGOSH_RUN{RestBatchMultipartHeader}
    var parts = [
      "Content-Type: application/x-arango-batchpart\r\n" +
      "Content-Id: myId1\r\n\r\n" +
      "GET /_api/version HTTP/1.1\r\n",

      "Content-Type: application/x-arango-batchpart\r\n" +
      "Content-Id: myId2\r\n\r\n" +
      "DELETE /_api/collection/products HTTP/1.1\r\n",

      "Content-Type: application/x-arango-batchpart\r\n" +
      "Content-Id: someId\r\n\r\n" +
      "POST /_api/collection/products HTTP/1.1\r\n\r\n" +
      "{\"name\": \"products\" }\r\n",

      "Content-Type: application/x-arango-batchpart\r\n" +
      "Content-Id: nextId\r\n\r\n" +
      "GET /_api/collection/products/figures HTTP/1.1\r\n",

      "Content-Type: application/x-arango-batchpart\r\n" +
      "Content-Id: otherId\r\n\r\n" +
      "DELETE /_api/collection/products HTTP/1.1\r\n"
    ];
    var boundary = "SomeBoundaryValue";
    var headers = { "Content-Type" : "multipart/form-data; boundary=" +
    boundary };
    var body = "--" + boundary + "\r\n" +
               parts.join("\r\n" + "--" + boundary + "\r\n") +
               "--" + boundary + "--\r\n";

    var response = logCurlRequestPlain('POST', '/_api/batch', body, headers);

    assert(response.code === 200);

    logPlainResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Sending a batch request, setting the boundary implicitly (the server will
in this case try to find the boundary at the beginning of the request body).

@EXAMPLE_ARANGOSH_RUN{RestBatchImplicitBoundary}
    var parts = [
      "Content-Type: application/x-arango-batchpart\r\n\r\n" +
         "DELETE /_api/collection/notexisting1 HTTP/1.1\r\n",
      "Content-Type: application/x-arango-batchpart\r\n\r\n" +
         "DELETE _api/collection/notexisting2 HTTP/1.1\r\n"
    ];
    var boundary = "SomeBoundaryValue";
    var body = "--" + boundary + "\r\n" +
               parts.join("\r\n" + "--" + boundary + "\r\n") +
               "--" + boundary + "--\r\n";

    var response = logCurlRequestPlain('POST', '/_api/batch', body);

    assert(response.code === 200);
    assert(response.headers['x-arango-errors'] == 2);

    logPlainResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
