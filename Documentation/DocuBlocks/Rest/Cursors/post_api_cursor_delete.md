
@startDocuBlock post_api_cursor_delete
@brief dispose an existing cursor

@RESTHEADER{DELETE /_api/cursor/{cursor-identifier}, Delete cursor, deleteQueryCursor}

@RESTURLPARAMETERS

@RESTURLPARAM{cursor-identifier,string,required}
The id of the cursor

@RESTDESCRIPTION
Deletes the cursor and frees the resources associated with it.

The cursor will automatically be destroyed on the server when the client has
retrieved all documents from it. The client can also explicitly destroy the
cursor at any earlier time using an HTTP DELETE request. The cursor id must
be included as part of the URL.

Note: the server will also destroy abandoned cursors automatically after a
certain server-controlled timeout to avoid resource leakage.

@RESTRETURNCODES

@RESTRETURNCODE{202}
is returned if the server is aware of the cursor.

@RESTRETURNCODE{404}
is returned if the server is not aware of the cursor. It is also
returned if a cursor is used after it has been destroyed.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCursorDelete}
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
    logJsonResponse(response);
    var body = response.body.replace(/\\/g, '');
    var _id = JSON.parse(body).id;
    response = logCurlRequest('DELETE', url + '/' + _id);

    assert(response.code === 202);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
