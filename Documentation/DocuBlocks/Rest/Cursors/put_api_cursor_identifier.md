
@startDocuBlock put_api_cursor_identifier
@brief return the next results from an existing cursor

@RESTHEADER{PUT /_api/cursor/{cursor-identifier}, Read next batch from cursor (deprecated), modifyQueryCursorPut}

@HINTS
{% hint 'warning' %}
There is a functionally equivalent HTTP POST counterpart to this endpoint, which should
be used preferrably.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{cursor-identifier,string,required}
The name of the cursor

@RESTDESCRIPTION
If the cursor is still alive, returns an object with the following
attributes:

- *id*: the *cursor-identifier*
- *result*: a list of documents for the current batch
- *hasMore*: *false* if this was the last batch
- *count*: if present the total number of elements

Note that even if *hasMore* returns *true*, the next call might
still return no documents. If, however, *hasMore* is *false*, then
the cursor is exhausted.  Once the *hasMore* attribute has a value of
*false*, the client can stop.

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server will respond with *HTTP 200* in case of success.

@RESTRETURNCODE{400}
If the cursor identifier is omitted, the server will respond with *HTTP 404*.

@RESTRETURNCODE{404}
If no cursor with the specified identifier can be found, the server will respond
with *HTTP 404*.

@RESTRETURNCODE{410}
The server will respond with *HTTP 410* if a server which will process the query
or is the leader for a shard which is used in the query stops responding, but 
the connection has not been closed.

@RESTRETURNCODE{503}
The server will respond with *HTTP 503* if a server which will process the query
or is the leader for a shard which is used in the query is down, either for 
going through a restart, a failure or connectivity issues.


@EXAMPLES

Valid request for next batch

@EXAMPLE_ARANGOSH_RUN{RestCursorPutForLimitReturnCont}
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
    response = logCurlRequest('PUT', url + '/' + _id, '');
    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Missing identifier

@EXAMPLE_ARANGOSH_RUN{RestCursorPutMissingCursorIdentifier}
    var url = "/_api/cursor";

    var response = logCurlRequest('PUT', url, '');

    assert(response.code === 400);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Unknown identifier

@EXAMPLE_ARANGOSH_RUN{RestCursorPutInvalidCursorIdentifier}
    var url = "/_api/cursor/123123";

    var response = logCurlRequest('PUT', url, '');

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
