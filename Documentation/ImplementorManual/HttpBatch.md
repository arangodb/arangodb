HTTP Interface for Batch Requests {#HttpBatch}
==============================================

@NAVIGATE_HttpBatch
@EMBEDTOC{HttpBatchTOC}

Clients normally send individual operations to ArangoDB in individual
HTTP requests. This is straightforward and simple, but has the
disadvantage that the network overhead can be significant if many
small requests are issued in a row.

To mititage this problem, ArangoDB offers a batch request API that
clients can use to send multiple operations in one batch to
ArangoDB. This method is especially useful when the client has to send
many HTTP requests with a small body/payload and the individual
request results do not depend on each other.

Clients can use ArangoDB's batch API by issuing a multipart HTTP POST
request to the URL `/_api/batch` handler. The handler will accept the
request if the Content-Type is `multipart/form-data` and a boundary
string is specified. ArangoDB will then decompose the batch request
into its individual parts using this boundary. This also means that
the boundary string itself must not be contained in any of the parts.
When ArangoDB has split the multipart request into its individual
parts, it will process all parts sequentially as if it were a
standalone request.  When all parts are processed, ArangoDB will
generate a multipart HTTP response that contains one part for each
part operation result.  For example, if you send a multipart request
with 5 parts, ArangoDB will send back a multipart response with 5
parts as well.

The server expects each part message to start with exactly the
following "header": 

    Content-Type: application/x-arango-batchpart

You can optionally specify a `Content-Id` "header" to uniquely
identify each part message. The server will return the `Content-Id` in
its response if it is specified. Otherwise, the server will not send a
Content-Id "header" back. The server will not validate the uniqueness
of the Content-Id.  After the mandatory `Content-Type` and the
optional `Content-Id` header, two Windows linebreaks
(i.e. `\\r\\n\\r\\n`) must follow.  Any deviation of this structure
might lead to the part being rejected or incorrectly interpreted. The
part request payload, formatted as a regular HTTP request, must follow
the two Windows linebreaks literal directly.

Note that the literal `Content-Type: application/x-arango-batchpart`
technically is the header of the MIME part, and the HTTP request
(including its headers) is the body part of the MIME part.

An actual part request should start with the HTTP method, the called
URL, and the HTTP protocol version as usual, followed by arbitrary
HTTP headers. Its body should follow after the usual `\\r\\n\\r\\n`
literal. Part requests are therefore regular HTTP requests, only
embedded inside a multipart message.

The following example will send a batch with 3 individual document
creation operations. The boundary used in this example is
`XXXsubpartXXX`.

@verbinclude api-batch-simple

The server will then respond with one multipart message, containing
the overall status and the individual results for the part
operations. The overall status should be 200 except there was an error
while inspecting and processing the multipart message. The overall
status therefore does not indicate the success of each part operation,
but only indicates whether the multipart message could be handled
successfully.

Each part operation will return its own status value. As the part
operation results are regular HTTP responses (just included in one
multipart response), the part operation status is returned as a HTTP
status code. The status codes of the part operations are exactly the
same as if you called the individual operations standalone. Each part
operation might also return arbitrary HTTP headers and a body/payload:

@verbinclude api-batch-simple-response

In the above example, the server returned an overall status code of
200, and each part response contains its own status value (202 in the
example):

When constructing the multipart HTTP response, the server will use the
same boundary that the client supplied. If any of the part responses
has a status code of 400 or greater, the server will also return an
HTTP header `x-arango-errors` containing the overall number of part
requests that produced errors:

@verbinclude api-batch-fail

In this example, the overall response code is 200, but as some of the
part request failed (with status code 404), the `x-arango-errors`
header of the overall response is `1`:

@verbinclude api-batch-fail-response

@BNAVIGATE_HttpBatch
