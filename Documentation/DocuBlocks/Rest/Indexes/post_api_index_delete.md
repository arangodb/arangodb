
@startDocuBlock post_api_index_delete
@brief deletes an index

@RESTHEADER{DELETE /_api/index/{index-id}, Delete index, dropIndex}

@RESTURLPARAMETERS

@RESTURLPARAM{index-id,string,required}
The index id.

@RESTDESCRIPTION
Deletes an index with *index-id*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the index could be deleted, then an *HTTP 200* is
returned.

@RESTRETURNCODE{404}
If the *index-id* is unknown, then an *HTTP 404* is returned.
@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestIndexDeleteUniqueSkiplist}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index/" + db.products.ensureSkiplist("a","b").id;

    var response = logCurlRequest('DELETE', url);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
