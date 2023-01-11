
@startDocuBlock post_api_cursor_identifier
@brief Return the next results from an existing cursor

@RESTHEADER{POST /_api/cursor/{cursor-identifier}, Read next batch from cursor, modifyQueryCursorPost}

@RESTURLPARAMETERS

@RESTURLPARAM{cursor-identifier,string,required}
The ID of the cursor.

@RESTDESCRIPTION
If the cursor is still alive, returns an object with a batch of query results.

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
If the cursor identifier is omitted, the server responds with *HTTP 404*.

@RESTREPLYBODY{error,boolean,required,}
A flag to indicate that an error occurred (`false` in this case).

@RESTREPLYBODY{code,integer,required,integer}
The HTTP status code.

@RESTREPLYBODY{errorNum,integer,required,int64}
A server error number (if `error` is `true`).

@RESTREPLYBODY{errorMessage,string,required,string}
A descriptive error message (if `error` is `true`).

@RESTRETURNCODE{404}
If no cursor with the specified identifier can be found, the server responds
with *HTTP 404*.

@RESTRETURNCODE{410}
The server responds with *HTTP 410* if a server which processes the query
or is the leader for a shard which is used in the query stops responding, but 
the connection has not been closed.

@RESTRETURNCODE{503}
The server responds with *HTTP 503* if a server which processes the query
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

    var body = response.body.replace(/\\/g, '');
    var _id = JSON.parse(body).id;
    response = logCurlRequest('POST', url + '/' + _id, '');
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
