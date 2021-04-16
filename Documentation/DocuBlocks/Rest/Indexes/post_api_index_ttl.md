
@startDocuBlock post_api_index_ttl
@brief creates a TTL (time-to-live) index

@RESTHEADER{POST /_api/index#ttl, Create TTL index, createIndex:ttl}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The collection name.

@RESTBODYPARAM{type,string,required,string}
must be equal to *"ttl"*.

@RESTBODYPARAM{fields,array,required,string}
an array with exactly one attribute path.

@RESTBODYPARAM{expireAfter,number,required,}
The time interval (in seconds) from the point in time stored in the `fields`
attribute after which the documents count as expired. Can be set to `0` to let
documents expire as soon as the server time passes the point in time stored in
the document attribute, or to a higher number to delay the expiration.

@RESTDESCRIPTION
Creates a TTL index for the collection *collection-name* if it
does not already exist. The call expects an object containing the index
details.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the index already exists, then a *HTTP 200* is returned.

@RESTRETURNCODE{201}
If the index does not already exist and could be created, then a *HTTP 201*
is returned.

@RESTRETURNCODE{400}
If the collection already contains another TTL index, then an *HTTP 400* is
returned, as there can be at most one TTL index per collection.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Creating a TTL index

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewTtlIndex}
    var cn = "sessions";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = {
      type: "ttl",
      expireAfter: 3600,
      fields : [ "createdAt" ]
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
