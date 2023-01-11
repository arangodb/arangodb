
@startDocuBlock post_api_cursor_identifier_batch
@brief Return results from an existing cursor once more

@RESTHEADER{POST /_api/cursor/{cursor-identifier}/{batch-identifier}, Read batch from cursor again, batchQueryCursorPost}

@RESTURLPARAMETERS

@RESTURLPARAM{cursor-identifier,string,required}
The ID of the cursor.

@RESTURLPARAM{batch-identifier,string,required}
The ID of the batch. The first batch has an ID of `1` and the value is
incremented by 1 with every batch. You can only request the latest batch again.
Earlier batches are not kept on the server-side.

@RESTDESCRIPTION
You can use this endpoint to retry fetching the latest batch from a cursor.
The endpoint requires the `retriable` query option to be enabled for the cursor.

If the cursor is still alive, returns an object with a batch of query results.
Calling this endpoint does not advance the cursor.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server responds with *HTTP 200* in case of success.

@RESTREPLYBODY{error,boolean,required,}
A flag to indicate that an error occurred (`false` in this case).

@RESTREPLYBODY{code,integer,required,integer}
The HTTP status code.

@RESTREPLYBODY{result,array,optional,}
An array of result documents for the current batch.

@RESTREPLYBODY{hasMore,boolean,required,}
A boolean indicator whether there are more results available for the cursor on
the server. `false` if this is the last batch.

Note that even if `hasMore` returns `true`, the next call might still return no
documents. If, however, `hasMore` is `false`, then the cursor is exhausted.
Once the `hasMore` attribute has a value of `false`, the client can stop.

@RESTREPLYBODY{count,integer,optional,int64}
The total number of result documents available (only
available if the query was executed with the `count` attribute set).

@RESTREPLYBODY{id,string,optional,string}
The ID of the cursor for fetching more result batches.

@RESTREPLYBODY{extra,object,optional,}
An optional JSON object with extra information about the query result. It can
have the attributes `stats`, `warnings`, `plan`, and `profile` with nested
objects, like the initial cursor response. Every batch can include `warnings`.
The other attributes are only delivered as part of the last batch in case of a
cursor with the `stream` option enabled.

@RESTRETURNCODE{400}
If the cursor and the batch identifier are omitted, the server responds with
*HTTP 400*.

@RESTREPLYBODY{error,boolean,required,}
A flag to indicate that an error occurred (`false` in this case).

@RESTREPLYBODY{code,integer,required,integer}
The HTTP status code.

@RESTREPLYBODY{errorNum,integer,required,int64}
A server error number (if `error` is `true`).

@RESTREPLYBODY{errorMessage,string,required,string}
A descriptive error message (if `error` is `true`).

@RESTRETURNCODE{404}
If no cursor with the specified identifier can be found, or if the requested
batch isn't available, the server responds with *HTTP 404*.

@RESTRETURNCODE{410}
The server responds with *HTTP 410* if a server which processes the query
or is the leader for a shard which is used in the query stops responding, but 
the connection has not been closed.

@RESTRETURNCODE{503}
The server responds with *HTTP 503* if a server which processes the query
or is the leader for a shard which is used in the query is down, either for 
going through a restart, a failure or connectivity issues.

@EXAMPLES

Request the second batch (again):

@EXAMPLE_ARANGOSH_RUN{RestCursorPostBatch}
    var url = "/_api/cursor";
    var body = {
      query: "FOR i IN 1..5 RETURN i",
      count: true,
      batchSize: 2,
      options: {
        retriable: true
      }
    };
    var response = logCurlRequest('POST', url, body);
    var secondBatchId = response.parsedBody.nextBatchId;
    assert(response.code === 201);
    logJsonResponse(response);

    response = logCurlRequest('POST', url + '/' + response.parsedBody.id, '');
    assert(response.code === 200);
    logJsonResponse(response);

    response = logCurlRequest('POST', url + '/' + response.parsedBody.id + '/' + secondBatchId, '');
    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

