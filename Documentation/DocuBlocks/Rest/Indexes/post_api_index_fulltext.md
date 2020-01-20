
@startDocuBlock post_api_index_fulltext
@brief creates a fulltext index

@RESTHEADER{POST /_api/index#fulltext, Create fulltext index, createIndex#fulltext}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The collection name.

@RESTBODYPARAM{type,string,required,string}
must be equal to *"fulltext"*.

@RESTBODYPARAM{fields,array,required,string}
an array of attribute names. Currently, the array is limited
to exactly one attribute.

@RESTBODYPARAM{minLength,integer,required,int64}
Minimum character length of words to index. Will default
to a server-defined value if unspecified. It is thus recommended to set
this value explicitly when creating the index.

@RESTDESCRIPTION
Creates a fulltext index for the collection *collection-name*, if
it does not already exist. The call expects an object containing the index
details.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the index already exists, then a *HTTP 200* is
returned.

@RESTRETURNCODE{201}
If the index does not already exist and could be created, then a *HTTP 201*
is returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Creating a fulltext index

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewFulltext}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = {
      type: "fulltext",
      fields: [ "text" ]
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
