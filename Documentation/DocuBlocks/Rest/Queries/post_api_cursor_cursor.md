
@startDocuBlock post_api_cursor_cursor

@RESTHEADER{POST /_api/cursor/{cursor-identifier}, Read the next batch from a cursor, getNextAqlQueryCursorBatch}

@RESTURLPARAMETERS

@RESTURLPARAM{cursor-identifier,string,required}
The name of the cursor

@RESTDESCRIPTION
If the cursor is still alive, returns an object with the following
attributes:

- `id`: a `cursor-identifier`
- `result`: a list of documents for the current batch
- `hasMore`: `false` if this was the last batch
- `count`: if present the total number of elements
- `code`: an HTTP status code
- `error`: a boolean flag to indicate whether an error occurred
- `errorNum`: a server error number (if `error` is `true`)
- `errorMessage`: a descriptive error message (if `error` is `true`)
- `extra`: an object with additional information about the query result, with
  the nested objects `stats` and `warnings`. Only delivered as part of the last
  batch in case of a cursor with the `stream` option enabled.

Note that even if `hasMore` returns `true`, the next call might
still return no documents. If, however, `hasMore` is `false`, then
the cursor is exhausted.  Once the `hasMore` attribute has a value of
`false`, the client can stop.

If the cursor is not fully consumed, the time-to-live for the cursor
is renewed by this API call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server will respond with *HTTP 200* in case of success.

@RESTREPLYBODY{nextBatchId,string,optional,string}
Only set if the `allowRetry` query option is enabled.

The ID of the batch after the current one. The first batch has an ID of `1` and
the value is incremented by 1 with every batch. You can remember and use this
batch ID should retrieving the next batch fail. Use the
`POST /_api/cursor/<cursor-id>/<batch-id>` endpoint to ask for the batch again.

@RESTRETURNCODE{400}
If the cursor identifier is omitted, the server will respond with *HTTP 404*.

@RESTRETURNCODE{404}
If no cursor with the specified identifier can be found, the server will respond
with *HTTP 404*.

@RESTRETURNCODE{410}
The server will respond with *HTTP 410* if a server which processes the query
or is the leader for a shard which is used in the query stops responding, but 
the connection has not been closed.

@RESTRETURNCODE{503}
The server will respond with *HTTP 503* if a server which processes the query
or is the leader for a shard which is used in the query is down, either for 
going through a restart, a failure or connectivity issues.


@EXAMPLES

Valid request for next batch

@EXAMPLE_ARANGOSH_RUN{RestCursorPostForLimitReturnCont}
    var url = "/_api/cursor";
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    db.products.save({"hello1":"world1"});
    db.products.save({"hello2":"world1"});
    db.products.save({"hello3":"world1"});
    db.products.save({"hello4":"world1"});
    db.products.save({"hello5":"world1"});

    var url = "/_api/cursor";
    var body = {
      query: "FOR p IN products LIMIT 5 RETURN p",
      count: true,
      batchSize: 2
    };
    var response = logCurlRequest('POST', url, body);

    response = logCurlRequest('POST', url + '/' + response.parsedBody.id, '');
    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Missing identifier

@EXAMPLE_ARANGOSH_RUN{RestCursorPostMissingCursorIdentifier}
    var url = "/_api/cursor";

    var response = logCurlRequest('POST', url, '');

    assert(response.code === 400);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Unknown identifier

@EXAMPLE_ARANGOSH_RUN{RestCursorPostInvalidCursorIdentifier}
    var url = "/_api/cursor/123123";

    var response = logCurlRequest('POST', url, '');

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
