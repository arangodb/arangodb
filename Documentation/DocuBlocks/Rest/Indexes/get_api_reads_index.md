
@startDocuBlock get_api_reads_index
@brief returns an index

@RESTHEADER{GET /_api/index/{index-id},Read index, getIndexes:handle}

@RESTURLPARAMETERS

@RESTURLPARAM{index-id,string,required}
The index identifier.

@RESTDESCRIPTION
The result is an object describing the index. It has at least the following
attributes:

- *id*: the identifier of the index

- *type*: the index type

All other attributes are type-dependent. For example, some indexes provide
*unique* or *sparse* flags, whereas others don't. Some indexes also provide
a selectivity estimate in the *selectivityEstimate* attribute of the result.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the index exists, then a *HTTP 200* is returned.

@RESTRETURNCODE{404}
If the index does not exist, then a *HTTP 404*
is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestIndexPrimaryIndex}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index/" + cn + "/0";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
