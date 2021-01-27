
@startDocuBlock get_api_collection_figures
@brief Fetch the statistics of a collection

@RESTHEADER{GET /_api/collection/{collection-name}/figures, Return statistics for a collection, handleCommandGet:collectionFigures}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{details,boolean,optional}
Setting `details` to `true` will return extended storage engine-specific
details to the figures. The details are intended for debugging ArangoDB itself
and their format is subject to change. By default, `details` is set to `false`,
so no details are returned and the behavior is identical to previous versions
of ArangoDB.
Please note that requesting `details` may cause additional load and thus have
an impact on performace.

@RESTDESCRIPTION
In addition to the above, the result also contains the number of documents
and additional statistical information about the collection.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returns information about the collection:

@RESTREPLYBODY{count,integer,required,int64}
The number of documents currently present in the collection.

@RESTREPLYBODY{figures,object,required,collection_figures}
metrics of the collection

@RESTSTRUCT{indexes,collection_figures,object,required,collection_figures_indexes}

@RESTSTRUCT{count,collection_figures_indexes,integer,required,int64}
The total number of indexes defined for the collection, including the pre-defined
indexes (e.g. primary index).

@RESTSTRUCT{size,collection_figures_indexes,integer,required,int64}
The total memory allocated for indexes in bytes.

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

Using an identifier and requesting the figures of the collection:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionFigures}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn);
    coll.save({"test":"hello"});
    require("internal").wal.flush(true, true);
    var url = "/_api/collection/"+ coll.name() + "/figures";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionFiguresDetails}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn);
    coll.save({"test":"hello"});
    require("internal").wal.flush(true, true);
    var url = "/_api/collection/"+ coll.name() + "/figures?details=true";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
