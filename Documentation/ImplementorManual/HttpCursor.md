HTTP Interface for AQL Query Cursors {#HttpCursor}
==================================================

@NAVIGATE_HttpCursor
@EMBEDTOC{HttpCursorTOC}

Database Cursors {#HttpCursorIntro}
===================================

This is an introduction to ArangoDB's Http interface for Queries. Results of AQL
and simple queries are returned as cursors in order to batch the communication
between server and client. Each call returns a number of documents in a batch
and an indication, if the current batch has been the final batch. Depending on
the query, the total number of documents in the result set might or might not be
known in advance. In order to free server resources the client should delete the
cursor as soon as it is no longer needed.

To run a select query, the query details need to be shipped from the client to
the server via a HTTP POST request.

Retrieving query results {#HttpCursorResults}
=============================================

Select queries are executed on-the-fly on the server and the result
set will be returned back to the client.

There are two ways the client can get the result set from the server:

- in a single roundtrip
- using a cursor

Single roundtrip {#HttpCursorResultsSingle}
-------------------------------------------

The server will only transfer a certain number of result documents back to the
client in one roundtrip. This number is controllable by the client by setting
the `batchSize` attribute when issueing the query.

If the complete result can be transferred to the client in one go, the client
does not need to issue any further request. The client can check whether it has
retrieved the complete result set by checking the `hasMore` attribute of the
result set. If it is set to `false`, then the client has fetched the complete
result set from the server. In this case no server side cursor will be created.

@EXAMPLES

@verbinclude api-cursor-create-for-limit-return-single

Using a Cursor {#HttpCursorResultsCursor}
-----------------------------------------

If the result set contains more documents than should be transferred in a single
roundtrip (i.e. as set via the `batchSize` attribute), the server will return
the first few documents and create a temporary cursor. The cursor identifier
will also be returned to the client. The server will put the cursor identifier
in the `id` attribute of the response object. Furthermore, the `hasMore`
attribute of the response object will be set to `true`. This is an indication
for the client that there are additional results to fetch from the server.

@EXAMPLES

Create and extract first batch:

@verbinclude api-cursor-create-for-limit-return

Extract next batch, still have more:

@verbinclude api-cursor-create-for-limit-return-cont

Extract next batch, done:

@verbinclude api-cursor-create-for-limit-return-cont2

Do not do this:

@verbinclude api-cursor-create-for-limit-return-cont3

Accessing Cursors via HTTP {#HttpCursorHttp}
============================================

@anchor HttpCursorPost
@copydetails JSF_post_api_cursor

@CLEARPAGE
@anchor HttpCursorPostQuery
@copydetails JSF_post_api_query

@CLEARPAGE
@anchor HttpCursorPut
@copydetails JSF_put_api_cursor

@CLEARPAGE
@anchor HttpCursorDelete
@copydetails JSF_delete_api_cursor

