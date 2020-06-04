
@startDocuBlock put_api_simple_within
@brief returns all documents of a collection within a given radius

@RESTHEADER{PUT /_api/simple/within, Find documents within a radius around a coordinate}

@HINTS
{% hint 'warning' %}
This route should no longer be used.
All endpoints for Simple Queries are deprecated from version 3.4.0 on.
They are superseded by AQL queries.
{% endhint %}

@RESTBODYPARAM{collection,string,required,string}
The name of the collection to query.

@RESTBODYPARAM{latitude,string,required,string}
The latitude of the coordinate.

@RESTBODYPARAM{longitude,string,required,string}
The longitude of the coordinate.

@RESTBODYPARAM{radius,string,required,string}
The maximal radius (in meters).

@RESTBODYPARAM{distance,string,required,string}
If given, the attribute key used to return the distance to
the given coordinate. (optional). If specified, distances are returned in meters.

@RESTBODYPARAM{skip,string,required,string}
The number of documents to skip in the query. (optional)

@RESTBODYPARAM{limit,string,required,string}
The maximal amount of documents to return. The *skip* is
applied before the *limit* restriction. The default is 100. (optional)

@RESTBODYPARAM{geo,string,required,string}
If given, the identifier of the geo-index to use. (optional)

@RESTDESCRIPTION

This will find all documents within a given radius around the coordinate
(*latitude*, *longitude*). The returned list is sorted by distance.

In order to use the *within* operator, a geo index must be defined for
the collection. This index also defines which attribute holds the
coordinates for the document.  If you have more than one geo-spatial index,
you can use the *geo* field to select a particular index.

Returns a cursor containing the result, see [HTTP Cursor](../AqlQueryCursor/README.md) for details.

Note: the *within* simple query is **deprecated** as of ArangoDB 2.6.
This API may be removed in future versions of ArangoDB. The preferred
way for retrieving documents from a collection using the near operator is
to issue an [AQL query](../../AQL/Functions/Geo.html) using the *WITHIN* function as follows:

    FOR doc IN WITHIN(@@collection, @latitude, @longitude, @radius, @distanceAttributeName)
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

Without distance

@EXAMPLE_ARANGOSH_RUN{RestSimpleWithin}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    var loc = products.ensureGeoIndex("loc");
    var i;
    for (i = -0.01;  i <= 0.01;  i += 0.002) {
      products.save({ name : "Name/" + i + "/",loc: [ i, 0 ] });
    }
    var url = "/_api/simple/near";
    var body = {
      "collection": "products",
      "latitude" : 0,
      "longitude" : 0,
      "skip" : 1,
      "limit" : 2,
      "radius" : 500
    };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

With distance

@EXAMPLE_ARANGOSH_RUN{RestSimpleWithinDistance}
    var cn = "products";
    db._drop(cn);
    var products = db._create(cn);
    var loc = products.ensureGeoIndex("loc");
    var i;
    for (i = -0.01;  i <= 0.01;  i += 0.002) {
      products.save({ name : "Name/" + i + "/",loc: [ i, 0 ] });
    }
    var url = "/_api/simple/near";
    var body = { "collection": "products", "latitude" : 0, "longitude" : 0,
               "skip" : 1, "limit" : 3, "distance" : "distance", "radius" : 300 };

    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
