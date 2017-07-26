
@startDocuBlock JSA_get_api_collection_properties
@brief reads the properties of the specified collection

@RESTHEADER{GET /_api/collection/{collection-name}/properties, Read properties of a collection}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
In addition to the above, the result will always contain the
*waitForSync* attribute, and the *doCompact*, *journalSize*, 
and *isVolatile* attributes for the MMFiles storage engine.
This is achieved by forcing a load of the underlying collection.

- *waitForSync*: If *true* then creating, changing or removing
  documents will wait until the data has been synchronized to disk.

- *doCompact*: Whether or not the collection will be compacted.
  This option is only present for the MMFiles storage engine.

- *journalSize*: The maximal size setting for journals / datafiles
  in bytes.
  This option is only present for the MMFiles storage engine.

- *keyOptions*: JSON object which contains key generation options:
  - *type*: specifies the type of the key generator. The currently
    available generators are *traditional* and *autoincrement*.
  - *allowUserKeys*: if set to *true*, then it is allowed to supply
    own key values in the *_key* attribute of a document. If set to
    *false*, then the key generator is solely responsible for
    generating keys and supplying own key values in the *_key* attribute
    of documents is considered an error.

- *isVolatile*: If *true* then the collection data will be
  kept in memory only and ArangoDB will not write or sync the data
  to disk.
  This option is only present for the MMFiles storage engine.

In a cluster setup, the result will also contain the following attributes:
- *numberOfShards*: the number of shards of the collection.

- *shardKeys*: contains the names of document attributes that are used to
  determine the target shard for documents.

- *replicationFactor*: contains how many copies of each shard are kept on different DBServers.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

Using an identifier:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionIdentifier}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn, { waitForSync: true });
    var url = "/_api/collection/"+ coll._id + "/properties";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Using a name:

@EXAMPLE_ARANGOSH_RUN{RestCollectionGetCollectionName}
    var cn = "products";
    db._drop(cn);
    db._create(cn, { waitForSync: true });
    var url = "/_api/collection/products/properties";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

