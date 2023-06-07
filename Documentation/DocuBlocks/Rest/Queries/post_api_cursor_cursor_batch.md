
@startDocuBlock post_api_cursor_cursor_batch
@brief Return results from an existing cursor once more

@RESTHEADER{POST /_api/cursor/{cursor-identifier}/{batch-identifier}, Read batch from cursor again, getPreviousAqlQueryCursorBatch}

@RESTURLPARAMETERS

@RESTURLPARAM{cursor-identifier,string,required}
The ID of the cursor.

@RESTURLPARAM{batch-identifier,string,required}
The ID of the batch. The first batch has an ID of `1` and the value is
incremented by 1 with every batch. You can only request the latest batch again
(or the next batch). Earlier batches are not kept on the server-side.

@RESTDESCRIPTION
You can use this endpoint to retry fetching the latest batch from a cursor.
The endpoint requires the `allowRetry` query option to be enabled for the cursor.

Calling this endpoint with the last returned batch identifier returns the
query results for that same batch again. This does not advance the cursor.
Client applications can use this to re-transfer a batch once more in case of
transfer errors.

You can also call this endpoint with the next batch identifier, i.e. the value
returned in the `nextBatchId` attribute of a previous request. This advances the
cursor and returns the results of the next batch.

From v3.11.1 onward, you may use this endpoint even if the `allowRetry`
attribute is `false` to fetch the next batch, but you cannot request a batch
again unless you set it to `true`.

Note that it is only supported to query the last returned batch identifier or
the directly following batch identifier. The latter is only supported if there
are more results in the cursor (i.e. `hasMore` is `true` in the latest batch).

Note that when the last batch has been consumed successfully by a client
application, it should explicitly delete the cursor to inform the server that it
successfully received and processed the batch so that the server can free up
resources.

The time-to-live for the cursor is renewed by this API call.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server responds with *HTTP 200* in case of success.

@RESTREPLYBODY{,object,required,post_api_cursor_result}

@RESTRETURNCODE{400}
If the cursor and the batch identifier are omitted, the server responds with
*HTTP 400*.

@RESTREPLYBODY{error,boolean,required,}
A flag to indicate that an error occurred (`false` in this case).

@RESTREPLYBODY{code,integer,required,int64}
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
        allowRetry: true
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

