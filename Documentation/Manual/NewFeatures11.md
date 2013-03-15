New Features in ArangoDB 1.1 {#NewFeatures11}
=============================================

@NAVIGATE_NewFeatures11
@EMBEDTOC{NewFeatures11TOC}

Features and Improvements {#NewFeatures11Introduction}
======================================================

The following list shows in detail which features have been added or
improved in ArangoDB 1.1.  Additionally, ArangoDB 1.1 contains a lot
of bugfixes that are not listed separately.

Collection Types {#NewFeatures11CollectionTypes}
------------------------------------------------

In ArangoDB 1.1, collections are now explicitly typed:

- regular documents go into _document_-only collections, 
- and edges go into _edge_ collections. 

In 1.0, collections were untyped, and edges and documents could be
mixed in the same collection.  Whether or not a collection was to be
treated as an _edge_ or _document_ collection was decided at runtime
by looking at the prefix used (e.g. `db.xxx` vs. `edges.xxx`).

The explicit collection types used in ArangoDB allow users to query
the collection type at runtime and make decisions based on the type:

    arangosh> db.users.type();

Extra Javascript functions have been introduced to create collections:

    arangosh> db._createDocumentCollection("users");
    arangosh> db._createEdgeCollection("relationships");

The "traditional" functions are still available:

    arangosh> db._create("users");
    arangosh> edges._create("relationships");

The ArangoDB web interface also allows the explicit creation of _edge_
collections.

Batch Requests {#NewFeatures11BatchRequests}
--------------------------------------------

ArangoDB 1.1 provides a new REST API for batch requests at
`/_api/batch`.

Clients can use the API to send multiple requests to ArangoDB at
once. They can package multiple requests into just one aggregated
request.

ArangoDB will then unpack the aggregated request and process the
contained requests one-by-one. When done it will send an aggregated
response to the client, that the client can then unpack to get the
list of individual responses.

Using the batch request API may save network overhead because it
reduces the number of HTTP requests and responses that clients and
ArangoDB need to exchange.  This may be especially important if the
network is slow or if the individual requests are small and the
network overhead per request would be significant.

It should be noted that packing multiple individual requests into one
aggregate request on the client side introduces some overhead
itself. The same is true for the aggregate request unpacking and
assembling on the server side. Using batch requests may still be
beneficial in many cases, but it should be obvious that they should be
used only when they replace a considerable amount of individual
requests.

For more information see @ref HttpBatch and
@EXTREF{http://www.arangodb.org/2012/10/04/gain-factor-of-5-using-batch-updates,this
blog article}.

Support for Partial Updates {#NewFeatures11PartialUpdates}
----------------------------------------------------------
  
The REST API for documents now offers the HTTP PATCH method to
partially update documents. A partial update allows specifying only
the attributes the change instead of the full document. Internally, it
will merge the supplied attributes into the existing document.

Completely overwriting/replacing entire documents is still available
via the HTTP PUT method in ArangoDB 1.0.  In _arangosh_, the partial
update method is named _update_, and the previously existing _replace_
method still performs a replacement of the entire document as before.

This call with replace just the `active` attribute of the document
`user`. All other attributes will remain unmodified. The document
revision number will of course be updated as updating creates a new
revision:

    arangosh> db.users.update(user, { "active" : false });
    
Contrary, the `replace` method will replace the entire existing
document with the data supplied. All other attributes will be
removed. Replacing will also create a new revision:

    arangosh> db.users.replace(user, { "active" : false });

For more information, please check @ref RestDocument.

AQL Improvements {#NewFeatures11AqlImprovements}
------------------------------------------------

The following functions have been added or extended in the ArangoDB
Query Language (AQL) in ArangoDB 1.1:

- `MERGE_RECURSIVE()`: new function that merges documents recursively. 
  Especially, it will merge sub-attributes, a functionality not provided 
  by the previously existing `MERGE()` function.
- `NOT_NULL()`: now works with any number of arguments and returns the
  first non-null argument.  If all arguments are `null`, the function
  will return `null`, too.
- `FIRST_LIST()`: new function that returns the first argument that is
  a list, and `null` if none of the arguments are lists.
- `FIRST_DOCUMENT()`: new function that returns the first argument
  that is a document, and `null` if none of the arguments are
  documents.
- `TO_LIST()`: converts the argument into a list.

Disk Synchronisation Improvements {#NewFeatures11DiskSynchronisation}
---------------------------------------------------------------------

### Synchronisation of Shape Data

ArangoDB 1.1 provides an option `--database.force-sync-shapes` that
controls whether shape data (information about document attriubte
names and attribute value types) should be synchronised to disk
directly after each write, or if synchronisation is allowed to happen
asynchronously.  The latter options allows ArangoDB to return faster
from operations that involve new document shapes.

In ArangoDB 1.0, shape information was always synchronised to disk,
and users did not have any options. The default value of
`--database.force-sync-shapes` in ArangoDB 1.1 is `true` so it is
fully compatible with ArangoDB 1.0.  However, in ArangoDB 1.1 the
direct synchronisation can be turned off by setting the value to
`false`. Direct synchronisation of shape data will then be disabled
for collections that have a `waitForSync` value of `false`.  Shape
data will always be synchronised directly for collections that have a
`waitForSync` value of `true`.

Still, ArangoDB 1.1 may need to perform less synchronisation when it
writes shape data (attribute names and attribute value types of
collection documents).

Users may benefit if they save documents with many different
structures (in terms of document attribute names and attribute value
types) in the same collection. If only small amounts of distinct
document shapes are used, the effect will not be noticable.


### Finer Control of Disk Sync Behavior for CRUD operations

ArangoDB stores all document data in memory-mapped files. When adding
new documents, updating existing documents or deleting documents,
these modifications are appended at the end of the currently used
memory-mapped datafile.

It is configurable whether ArangoDB should directly respond then and
synchronise the changes to disk asynchronously, or if it should force
the synchronisation before responding. The parameter to control this
is named `waitForSync` and can be set on a per-collection level.

Often, sychronisation is not required on collection level, but on
operation level.  ArangoDB 1.1 tries to improve on this by providing
extra parameters for the REST and Javascript _document_ and _edge_
modification operations.

This parameter can be used to force synchronisation for operations
that work on collections that have `waitForSync` set to `false`.

The following REST API methods support the parameter `waitForSync` to
force synchronisation:

* `POST /_api/document`: adding a document
* `POST /_api/edge`: adding an edge
* `PATCH /_api/document`: partially update a document
* `PATCH /_api/edge`: partially update an edge
* `PUT /_api/document`: replace a document
* `PUT /_api/edge`: replace an edge
* `DELETE /_api/document`: delete a document
* `DELETE /_api/edge`: delete an edge

If the `waitForSync` parameter is omitted or set to `false`, the
collection-level synchronisation behavior will be applied. Setting the
parameter to `true` will force synchronisation.

The following Javascript methods support forcing synchronisation, too:

* `save()`
* `update()`
* `relace()`
* `delete()`

Force synchronisation of a save operation:

    > db.users.save({"name":"foo"}, true);

If the second parameter is omitted or set to `false`, the
collection-level synchronisation behavior will be applied. Setting the
parameter to `true` will force synchronisation.

Server Statistics {#NewFeatures11ServerStatistics}
--------------------------------------------------

ArangoDB 1.1 allows querying the server status via the administration
front-end (see @ref UserManualWebInterfaceStatistics) or via REST API
methods.

The following methods are available:
- `GET /_admin/connection-statistics`: provides connection statistics
- `GET /_admin/request-statistics`: provides request statistics

Both methods return the current figures and historical values. The historical
figures are aggregated. They can be used to monitor the current server status as
well as to get an overview of how the figures developed over time and look for
trends.

The ArangoDB web interface is using these APIs to provide charts with the
server connection statistics figures. It has a new tab "Statistics" for this purpose.

For more information on the APIs, please refer to 
@S_EXTREF_S{http://www.arangodb.org/manuals/1.1/HttpSystem.html#HttpSystemConnectionStatistics,HttpSystemConnectionStatistics}
and 
@S_EXTREF{http://www.arangodb.org/manuals/1.1/HttpSystem.html#HttpSystemRequestStatistics,HttpSystemRequestStatistics}.

Endpoints and SSL support {#NewFeatures11Endpoints}
---------------------------------------------------

ArangoDB can now listen to incoming connections on one or many
"endpoint" of different types. In ArangoDB lingo, an endpoint is the
combination of a protocol and some configuration for it.

The currently supported protocol types are:

- `tcp`: for unencrypted connection over TCP/IP
- `ssl`: for secure connections using SSL over TCP/IP
- `unix`:  for connections over Unix domain sockets

You should note that the data transferred inside the protocol is still
HTTP, regardless of the chosen protocol. The endpoint protocol can
thus be understood as the envelope that all HTTP communication is
shipped inside.

To specify an endpoint, ArangoDB 1.1 introduces a new option
`--server.endpoint`. The values accepted by this option have the
following specification syntax:

- `tcp://host:port (HTTP over IPv4)`
- `tcp://[host]:port (HTTP over IPv6)`
- `ssl://host:port (HTTP over SSL-encrypted IPv4)`
- `ssl://[host]:port (HTTP over SSL-encrypted IPv6)`
- `unix:///path/to/socket (HTTP over Unix domain socket)`

### TCP endpoints

The configuration options for the `tcp` endpoint type are hostname/ip
address and an optional port number. If the port is ommitted, the
default port number of 8529 is used.

To make the server listen to connections coming in for IP
192.168.173.13 on TCP/IP port 8529:

    > bin/arangod --server.endpoint tcp://192.168.173.13:8529 

To make the server listen to connections coming in for IP 127.0.0.1
TCP/IP port 999:

    > bin/arangod --server.endpoint tcp://127.0.0.1:999 


### SSL endpoints

SSL endpoints can be used for secure, encrypted connections to
ArangoDB. The connection is secured using SSL. SSL is computationally
intensive so using it will result in an (unavoidable) performance
degradation when compared to plain-text requests.

The configuration options for the `ssl` endpoint type are the same as
for `tcp` endpoints.

To make the server listen to SSL connections coming in for IP
192.168.173.13 on TCP/IP port 8529:

    > bin/arangod --server.endpoint ssl://192.168.173.13:8529 

As multiple endpoints can be configured, ArangoDB can serve SSL and
non-SSL requests in parallel, provided they use different ports:

    > bin/arangod --server.endpoint tcp://192.168.173.13:8529 --server.endpoint ssl://192.168.173.13:8530

### Unix domain socket endpoints    

The `unix` endpoint type can only be used if clients are on the same
host as the _arangod_ server.  Connections will then be estabished
using a Unix domain socket, which is backed by a socket descriptor
file. This type of connection should be slightly more efficient than
TCP/IP.

The configuration option for a `unix` endpoint type is the socket
descriptor filename:

To make the server use a Unix domain socket with filename
`/var/run/arango.sock`:

    > bin/arangod --server.endpoint unix:///var/run/arango.sock

Improved HTTP Request Handling {#NewFeatures11RequestHandling}
--------------------------------------------------------------

### Error Handling

ArangoDB 1.1 better handles malformed HTTP requests than ArangoDB 1.0
did.  When it encounters an invalid HTTP request, it might answer with
some HTTP status codes that ArangoDB 1.0 did not use:

- `HTTP 411 Length Required` will be returned for requests that have a negative
  value in their `Content-Length` HTTP header.
- `HTTP 413 Request Entity Too Large` will be returned for too big requests. The
  maximum size is 512 MB at the moment.
- `HTTP 431 Request Header Field Too Large` will be returned for requests with too
  long HTTP headers. The maximum size per header field is 1 MB at the moment.

For requests that are not completely shipped from the client to the
server, the server will allow the client 90 seconds of time before
closing the dangling connection.

If the `Content-Length` HTTP header in an incoming request is set and
contains a value that is less than the length of the HTTP body sent,
the server will return a `HTTP 400 Bad Request`.

### Keep-Alive

In version 1.1, ArangoDB will behave as follows when it comes to HTTP
keep-alive:

- if a client sends a `Connection: close` HTTP header, the server will
  close the connection as requested
- if a client sends a `Connection: keep-alive` HTTP header, the server
  will not close the connection but keep it alive as requested
- if a client does not send any `Connection` HTTP header, the server
  will assume _keep-alive_ if the request was an HTTP/1.1 request, and
  _close_ if the request was an HTTP/1.0 request
- dangling keep-alive connections will be closed automatically by the
  server after a configurable amount of seconds. To adjust the value,
  use the new server option `--server.keep-alive-timeout`.
- Keep-alive can be turned off in ArangoDB by setting
  `--server.keep-alive-timeout` to a value of `0`.


### Configurable Backlog

ArangoDB 1.1 adds an option `--server.backlog-size` to configure the
system backlog size.  The backlog size controls the maximum number of
queued connections and is used by the listen() system call.

The default value in ArangoDB is 10, the maximum value is
platform-dependent.

Using V8 Options {#NewFeatures11V8Options}
------------------------------------------

To use arbitrary options the V8 engine provides, ArangoDB 1.1
introduces a new startup option `--javascript.v8-options`.  All
options that shall be passed to V8 without being interpreted by
ArangoDB can be put inside this option. ArangoDB itself will ignore
these options and will let V8 handle them.  It is up to V8 to handle
these options and complain about wrong options. In case of invalid
options, V8 may refuse to start, which will also abort the startup of
ArangoDB.

To get a list of all options that the V8 engine in the currently used
version of ArangoDB supports, you can use the value `--help`, which
will just be passed through to V8:

    > bin/arangod --javascript.v8-options "--help" /tmp/voctest


Other Improvements {#NewFeatures11Other}
----------------------------------------

### Smaller Hash Indexes

Some internal structures have been adjusted in ArangoDB 1.1 so that
hash index entries consume considerably less memory.

Installations may benefit if they use unique or non-unqiue hash
indexes on collections.


### arangoimp 

_arangoimp_ now allows specifiying the end-of-line (EOL) character of
the input file.  This allows better support for files created on
Windows systems with `\r\n` EOLs.

_arangoimp_ also supports importing input files in TSV format. TSV is
a simple separated format such as CSV, but with the tab character as
the separator, no quoting for values and thus no support for line
breaks inside the values.

### libicu

ArangoDB uses ICU - International Components for Unicode (icu-project.org)
for string sorting and string normalization.

ArangoDB 1.1 adds the option `--default-language` to select a locale for
sorting and comparing strings. The default locale is set to be the system 
locale on that platform. 

