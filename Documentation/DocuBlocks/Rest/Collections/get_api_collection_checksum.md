
@startDocuBlock get_api_collection_checksum
@brief returns a checksum for the specified collection

@RESTHEADER{GET /_api/collection/{collection-name}/checksum, Return checksum for the collection, handleCommandGet:collectionChecksum}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{withRevisions,boolean,optional}
Whether or not to include document revision ids in the checksum calculation.

@RESTQUERYPARAM{withData,boolean,optional}
Whether or not to include document body data in the checksum calculation.

@RESTDESCRIPTION
Will calculate a checksum of the meta-data (keys and optionally revision ids) and
optionally the document data in the collection.

The checksum can be used to compare if two collections on different ArangoDB
instances contain the same contents. The current revision of the collection is
returned too so one can make sure the checksums are calculated for the same
state of data.

By default, the checksum will only be calculated on the *_key* system attribute
of the documents contained in the collection. For edge collections, the system
attributes *_from* and *_to* will also be included in the calculation.

By setting the optional query parameter *withRevisions* to *true*, then revision
ids (*_rev* system attributes) are included in the checksumming.

By providing the optional query parameter *withData* with a value of *true*,
the user-defined document attributes will be included in the calculation too.
**Note**: Including user-defined attributes will make the checksumming slower.

The response is a JSON object with the following attributes:

- *checksum*: The calculated checksum as a number.

- *revision*: The collection revision id as a string.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

Retrieving the checksum of a collection:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionChecksum}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn);
    coll.save({ foo: "bar" });
    var url = "/_api/collection/" + coll.name() + "/checksum";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Retrieving the checksum of a collection including the collection data,
but not the revisions:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionChecksumNoRev}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn);
    coll.save({ foo: "bar" });
    var url = "/_api/collection/" + coll.name() + "/checksum?withRevisions=false&withData=true";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
