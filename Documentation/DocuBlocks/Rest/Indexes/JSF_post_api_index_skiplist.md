
@startDocuBlock JSF_post_api_index_skiplist
@brief creates a skip-list

@RESTHEADER{POST /_api/index#skiplist, Create skip list}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection-name,string,required}
The collection name.

@RESTBODYPARAM{type,string,required,string}
must be equal to *"skiplist"*.

@RESTBODYPARAM{fields,array,required,string}
an array of attribute paths.

@RESTBODYPARAM{unique,boolean,required,}
if *true*, then create a unique index.

@RESTBODYPARAM{sparse,boolean,required,}
if *true*, then create a sparse index.

@RESTBODYPARAM{deduplicate,boolean,optional,boolean}
if *false*, the deduplication of array values is turned off.

@RESTDESCRIPTION

Creates a skip-list index for the collection *collection-name*, if
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
skip-list index in such a way that there are documents violating the
uniqueness, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Creating a skiplist index

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewSkiplist}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = { 
      type: "skiplist", 
      unique: false, 
      fields: [ "a", "b" ] 
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Creating a sparse skiplist index

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateSparseSkiplist}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = { 
      type: "skiplist", 
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

