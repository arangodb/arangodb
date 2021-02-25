
@startDocuBlock put_api_simple_fulltext
@brief returns documents of a collection as a result of a fulltext query

@RESTHEADER{PUT /_api/simple/fulltext, Fulltext index query}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to query.

@RESTBODYPARAM{attribute,string,required,string}
The attribute that contains the texts.

@RESTBODYPARAM{query,string,required,string}
The fulltext query.

@RESTBODYPARAM{skip,string,required,string}
The number of documents to skip in the query (optional).

@RESTBODYPARAM{limit,string,required,string}
The maximal amount of documents to return. The *skip*
is applied before the *limit* restriction. (optional)

@RESTBODYPARAM{index,string,required,string}
The identifier of the fulltext-index to use.

@RESTDESCRIPTION

This will find all documents from the collection that match the fulltext
query specified in *query*.

In order to use the *fulltext* operator, a fulltext index must be defined
for the collection and the specified attribute.

Returns a cursor containing the result.

Note: the *fulltext* simple query is **deprecated** as of ArangoDB 2.6.
This API may be removed in future versions of ArangoDB. The preferred
way for retrieving documents from a collection using the near operator is
to issue an AQL query using the *FULLTEXT()* AQL function as follows:

    FOR doc IN FULLTEXT(@@collection, @attributeName, @queryString, @limit)
      RETURN doc

@RESTRETURNCODES

@RESTRETURNCODE{201}
is returned if the query was executed successfully.

@RESTRETURNCODE{400}
is returned if the body does not contain a valid JSON representation of a
query. The response body contains an error document in this case.

@RESTRETURNCODE{404}
is returned if the collection specified by *collection* is unknown.  The
response body contains an error document in this case.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestSimpleFulltext}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    products.save({"text" : "this text contains word" });
    products.save({"text" : "this text also has a word" });
    products.save({"text" : "this is nothing" });
    var loc = products.ensureFulltextIndex("text");
    var url = "/_api/simple/fulltext";
    var body = { "collection": "products", "attribute" : "text", "query" : "word" };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
