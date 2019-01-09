
@startDocuBlock put_api_collection_properties
@brief changes a collection

@RESTHEADER{PUT /_api/collection/{collection-name}/properties, Change properties of a collection}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
Changes the properties of a collection. Expects an object with the
attribute(s)

- *waitForSync*: If *true* then creating or changing a
  document will wait until the data has been synchronized to disk.

- *journalSize*: The maximal size of a journal or datafile in bytes. 
  The value must be at least `1048576` (1 MB). Note that when
  changing the journalSize value, it will only have an effect for
  additional journals or datafiles that are created. Already
  existing journals or datafiles will not be affected.

On success an object with the following attributes is returned:

- *id*: The identifier of the collection.

- *name*: The name of the collection.

- *waitForSync*: The new value.

- *journalSize*: The new value.

- *status*: The status of the collection as number.

- *type*: The collection type. Valid types are:
  - 2: document collection
  - 3: edges collection

- *isSystem*: If *true* then the collection is a system collection.

- *isVolatile*: If *true* then the collection data will be
  kept in memory only and ArangoDB will not write or sync the data
  to disk.

- *doCompact*: Whether or not the collection will be compacted.

- *keyOptions*: JSON object which contains key generation options:
  - *type*: specifies the type of the key generator. The currently
    available generators are *traditional*, *autoincrement*, *uuid*
    and *padded*.
  - *allowUserKeys*: if set to *true*, then it is allowed to supply
    own key values in the *_key* attribute of a document. If set to
    *false*, then the key generator is solely responsible for
    generating keys and supplying own key values in the *_key* attribute
    of documents is considered an error.

**Note**: except for *waitForSync*, *journalSize* and *name*, collection
properties **cannot be changed** once a collection is created. To rename
a collection, the rename endpoint must be used.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the *collection-name* is missing, then a *HTTP 400* is
returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404*
is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestCollectionIdentifierPropertiesSync}
    var cn = "products";
    db._drop(cn);
    var coll = db._create(cn, { waitForSync: true });
    var url = "/_api/collection/"+ coll.name() + "/properties";

    var response = logCurlRequest('PUT', url, {"waitForSync" : true });

    assert(response.code === 200);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

