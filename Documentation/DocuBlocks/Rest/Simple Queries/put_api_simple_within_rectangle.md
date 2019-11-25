
@startDocuBlock put_api_simple_within_rectangle
@brief returns all documents of a collection within a rectangle

@RESTHEADER{PUT /_api/simple/within-rectangle, Within rectangle query}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to query.

@RESTBODYPARAM{latitude1,string,required,string}
The latitude of the first rectangle coordinate.

@RESTBODYPARAM{longitude1,string,required,string}
The longitude of the first rectangle coordinate.

@RESTBODYPARAM{latitude2,string,required,string}
The latitude of the second rectangle coordinate.

@RESTBODYPARAM{longitude2,string,required,string}
The longitude of the second rectangle coordinate.

@RESTBODYPARAM{skip,string,required,string}
The number of documents to skip in the query. (optional)

@RESTBODYPARAM{limit,string,required,string}
The maximal amount of documents to return. The *skip* is
applied before the *limit* restriction. The default is 100. (optional)

@RESTBODYPARAM{geo,string,required,string}
If given, the identifier of the geo-index to use. (optional)

@RESTDESCRIPTION

This will find all documents within the specified rectangle (determined by
the given coordinates (*latitude1*, *longitude1*, *latitude2*, *longitude2*).

In order to use the *within-rectangle* query, a geo index must be defined for
the collection. This index also defines which attribute holds the
coordinates for the document.  If you have more than one geo-spatial index,
you can use the *geo* field to select a particular index.

Returns a cursor containing the result, see [HTTP Cursor](../AqlQueryCursor/README.md) for details.

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

@EXAMPLE_ARANGOSH_RUN{RestSimpleWithinRectangle}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    var loc = products.ensureGeoIndex("loc");
    var i;
    for (i = -0.01;  i <= 0.01;  i += 0.002) {
      products.save({ name : "Name/" + i + "/",loc: [ i, 0 ] });
    }
    var url = "/_api/simple/within-rectangle";
    var body = {
      collection: "products",
      latitude1 : 0,
      longitude1 : 0,
      latitude2 : 0.2,
      longitude2 : 0.2,
      skip : 1,
      limit : 2
    };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
