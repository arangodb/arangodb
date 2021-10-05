
@startDocuBlock post_api_index_zkd
@brief creates a multi-dimensional index

@RESTHEADER{POST /_api/index#multi-dim, Create multi-dimensional index, createIndex#multi-dim}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The collection name.

@RESTBODYPARAM{type,string,required,string}
must be equal to *"zkd"*.

@RESTBODYPARAM{fields,array,required,string}
an array of attribute names used for each dimension. Array expansions are not allowed.

@RESTBODYPARAM{unique,boolean,optional,}
if *true*, then create a unique index.

@RESTBODYPARAM{inBackground,boolean,optional,}
The optional attribute **inBackground** can be set to *true* to create the index
in the background, which will not write-lock the underlying collection for
as long as if the index is built in the foreground. The default value is *false*.

@RESTBODYPARAM{fieldValueTypes,string,required,string}
must be equal to *"double"*. Currently only doubles are supported as values.

@RESTDESCRIPTION
Creates a multi-dimensional index for the collection *collection-name*, if
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

@RESTRETURNCODE{400}
If the index definition is invalid, then a *HTTP 400* is returned.

@EXAMPLES

Creating a multi-dimensional index

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateNewZkd}
var cn = "intervals";
db._drop(cn);
db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = {
      type: "zkd",
      fields: [ "from", "to" ],
      fieldValueTypes: "double"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
