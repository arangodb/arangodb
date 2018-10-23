
@startDocuBlock put_api_collection_rotate
@brief rotates the journal of a collection

@RESTHEADER{PUT /_api/collection/{collection-name}/rotate, Rotate journal of a collection}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
Rotates the journal of a collection. The current journal of the collection will be closed
and made a read-only datafile. The purpose of the rotate method is to make the data in
the file available for compaction (compaction is only performed for read-only datafiles, and
not for journals).

Saving new data in the collection subsequently will create a new journal file
automatically if there is no current journal.

It returns an object with the attributes

- *result*: will be *true* if rotation succeeded

**Note**: this method is specific for the MMFiles storage engine, and there
it is not available in a cluster.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the collection currently has no journal, *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Rotating the journal:

@EXAMPLE_ARANGOSH_RUN{RestCollectionRotate}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn);
    coll.save({ "test" : true });
    require("internal").wal.flush(true, true);

    var url = "/_api/collection/"+ coll.name() + "/rotate";
    var response = logCurlRequest('PUT', url, { });

    assert(response.code === 200);
    db._flushCache();
    db._drop(cn);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Rotating if no journal exists:

@EXAMPLE_ARANGOSH_RUN{RestCollectionRotateNoJournal}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn);
    var url = "/_api/collection/"+ coll.name() + "/rotate";

    var response = logCurlRequest('PUT', url, { });

    assert(response.code === 400);
    db._flushCache();
    db._drop(cn);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

