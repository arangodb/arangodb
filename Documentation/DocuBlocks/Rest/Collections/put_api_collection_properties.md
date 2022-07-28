
@startDocuBlock put_api_collection_properties
@brief changes a collection

@RESTHEADER{PUT /_api/collection/{collection-name}/properties, Change properties of a collection, handleCommandPut:modifyProperties}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
Changes the properties of a collection. Only the provided attributes are
updated. Collection properties **cannot be changed** once a collection is
created except for the listed properties, as well as the collection name via
the rename endpoint (but not in clusters).

@RESTBODYPARAM{waitForSync,boolean,optional,}
If *true* then the data is synchronized to disk before returning from a
document create, update, replace or removal operation. (default: false)

@RESTBODYPARAM{cacheEnabled,boolean,optional,}
Whether the in-memory hash cache for documents should be enabled for this
collection (default: *false*). Can be controlled globally with the `--cache.size`
startup option. The cache can speed up repeated reads of the same documents via
their document keys. If the same documents are not fetched often or are
modified frequently, then you may disable the cache to avoid the maintenance
costs.

@RESTBODYPARAM{schema,object,optional,}
Optional object that specifies the collection level schema for
documents. The attribute keys `rule`, `level` and `message` must follow the
rules documented in [Document Schema Validation](https://www.arangodb.com/docs/stable/data-modeling-documents-schema-validation.html)

@RESTBODYPARAM{computedValues,array,optional,put_api_collection_properties_computed_field}
An optional list of objects, each representing a computed value.

@RESTSTRUCT{name,put_api_collection_properties_computed_field,string,required,}
The name of the target attribute. Can only be a top-level attribute, but you
may return a nested object. Cannot be `_key`, `_id`, `_rev`, `_from`, `_to`,
or a shard key attribute.

@RESTSTRUCT{expression,put_api_collection_properties_computed_field,string,required,}
An AQL `RETURN` operation with an expression that computes the desired value.
See [Computed Value Expressions](https://www.arangodb.com/docs/devel/data-modeling-documents-computed-values.html#computed-value-expressions) for details.

@RESTSTRUCT{overwrite,put_api_collection_properties_computed_field,boolean,required,}
Whether the computed value shall take precedence over a user-provided or
existing attribute.

@RESTSTRUCT{computeOn,put_api_collection_properties_computed_field,array,optional,string}
An array of strings to define on which write operations the value shall be
computed. The possible values are `"insert"`, `"update"`, and `"replace"`.
The default is `["insert", "update", "replace"]`.

@RESTSTRUCT{keepNull,put_api_collection_properties_computed_field,boolean,optional,}
Whether the target attribute shall be set if the expression evaluates to `null`.
You can set the option to `false` to not set (or unset) the target attribute if
the expression returns `null`. The default is `true`.

@RESTSTRUCT{failOnWarning,put_api_collection_properties_computed_field,boolean,optional,}
Whether to let the write operation fail if the expression produces a warning.
The default is `false`.

@RESTBODYPARAM{replicationFactor,integer,optional,int64}
(The default is *1*): in a cluster, this attribute determines how many copies
of each shard are kept on different DB-Servers. The value 1 means that only one
copy (no synchronous replication) is kept. A value of k means that k-1 replicas
are kept. It can also be the string `"satellite"` for a SatelliteCollection,
where the replication factor is matched to the number of DB-Servers
(Enterprise Edition only).

Any two copies reside on different DB-Servers. Replication between them is
synchronous, that is, every write operation to the "leader" copy will be replicated
to all "follower" replicas, before the write operation is reported successful.

If a server fails, this is detected automatically and one of the servers holding
copies take over, usually without an error being reported.

@RESTBODYPARAM{writeConcern,integer,optional,int64}
Write concern for this collection (default: 1).
It determines how many copies of each shard are required to be
in sync on the different DB-Servers. If there are less then these many copies
in the cluster a shard will refuse to write. Writes to shards with enough
up-to-date copies will succeed at the same time however. The value of
*writeConcern* can not be larger than *replicationFactor*. _(cluster only)_

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
