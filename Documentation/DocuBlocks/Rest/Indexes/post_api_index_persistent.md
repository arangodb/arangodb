
@startDocuBlock post_api_index_persistent
@brief creates a persistent index

@RESTHEADER{POST /_api/index#persistent, Create a persistent index, createIndex:persistent}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The collection name.

@RESTBODYPARAM{type,string,required,string}
must be equal to *"persistent"*.

@RESTBODYPARAM{fields,array,required,string}
an array of attribute paths.

@RESTBODYPARAM{unique,boolean,optional,}
if *true*, then create a unique index.

@RESTBODYPARAM{sparse,boolean,optional,}
if *true*, then create a sparse index.

@RESTBODYPARAM{deduplicate,boolean,optional,}
The attribute **deduplicate** is supported by array indexes of type *persistent*,
*hash* or *skiplist*. It controls whether inserting duplicate index values
from the same document into a unique array index will lead to a unique constraint
error or not. The default value is *true*, so only a single instance of each
non-unique index value will be inserted into the index per document. Trying to
insert a value into the index that already exists in the index will always fail,
regardless of the value of this attribute.

@RESTBODYPARAM{estimates,boolean,optional,}
The attribute **estimates** is supported by indexes of type *persistent*. This
attribute controls whether index selectivity estimates are maintained for the
index. Not maintaining index selectivity estimates can have a slightly positive
impact on write performance.
The downside of turning off index selectivity estimates will be that
the query optimizer will not be able to determine the usefulness of different
competing indexes in AQL queries when there are multiple candidate indexes to
choose from.
The *estimates* attribute is optional and defaults to *true* if not set. It will
have no effect on indexes other than *persistent* (with *hash* and *skiplist*
being mere aliases for *persistent* nowadays).

@RESTDESCRIPTION

Creates a persistent index for the collection *collection-name*, if
it does not already exist. The call expects an object containing the index
details.

In a sparse index all documents will be excluded from the index that do not
contain at least one of the specified index attributes (i.e. *fields*) or that
have a value of *null* in any of the specified index attributes. Such documents
will not be indexed, and not be taken into account for uniqueness checks if
the *unique* flag is set.

In a non-sparse index, these documents will be indexed (for non-present
indexed attributes, a value of *null* will be used) and will be taken into
account for uniqueness checks if the *unique* flag is set.

**Note**: unique indexes on non-shard keys are not supported in a cluster.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the index already exists, then a *HTTP 200* is
returned.

@RESTRETURNCODE{201}
If the index does not already exist and could be created, then a *HTTP 201*
is returned.

@RESTRETURNCODE{400}
If the collection already contains documents and you try to create a unique
persistent index in such a way that there are documents violating the
uniqueness, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Creating a persistent index

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewPersistent}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = {
      type: "persistent",
      unique: false,
      fields: [ "a", "b" ]
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Creating a sparse persistent index

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateSparsePersistent}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = {
      type: "persistent",
      unique: false,
      sparse: true,
      fields: [ "a" ]
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
