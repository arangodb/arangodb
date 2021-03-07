
@startDocuBlock put_api_simple_range
@brief returns all documents of a collection within a range

@RESTHEADER{PUT /_api/simple/range, Simple range query}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to query.

@RESTBODYPARAM{attribute,string,required,string}
The attribute path to check.

@RESTBODYPARAM{left,string,required,string}
The lower bound.

@RESTBODYPARAM{right,string,required,string}
The upper bound.

@RESTBODYPARAM{closed,boolean,required,}
If *true*, use interval including *left* and *right*,
otherwise exclude *right*, but include *left*.

@RESTBODYPARAM{skip,string,required,string}
The number of documents to skip in the query (optional).

@RESTBODYPARAM{limit,integer,optional,int64}
The maximal amount of documents to return. The *skip*
is applied before the *limit* restriction. (optional)

@RESTDESCRIPTION

This will find all documents within a given range. In order to execute a
range query, a skip-list index on the queried attribute must be present.

Returns a cursor containing the result.

Note: the *range* simple query is **deprecated** as of ArangoDB 2.6.
The function may be removed in future versions of ArangoDB. The preferred
way for retrieving documents from a collection within a specific range
is to use an AQL query as follows:

    FOR doc IN @@collection
      FILTER doc.value >= @left && doc.value < @right
      LIMIT @skip, @limit
      RETURN doc`

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the query was executed successfully.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation of a
query. The response body contains an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection specified by *collection* is unknown or no
suitable index for the range query is present.  The response body contains
an error document in this case.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestSimpleRange}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.ensureUniqueSkiplist("i");
    products.save({ "i": 1});
    products.save({ "i": 2});
    products.save({ "i": 3});
    products.save({ "i": 4});
    var url = "/_api/simple/range";
    var body = { "collection": "products", "attribute" : "i", "left" : 2, "right" : 4 };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
