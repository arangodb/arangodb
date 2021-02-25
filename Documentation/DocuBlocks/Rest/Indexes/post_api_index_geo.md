
@startDocuBlock post_api_index_geo
@brief creates a geo index

@RESTHEADER{POST /_api/index#geo, Create geo-spatial index, createIndex#geo}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The collection name.

@RESTBODYPARAM{type,string,required,string}
must be equal to *"geo"*.

@RESTBODYPARAM{fields,array,required,string}
An array with one or two attribute paths.<br>
If it is an array with one attribute path *location*, then a geo-spatial
index on all documents is created using *location* as path to the
coordinates. The value of the attribute must be an array with at least two
double values. The array must contain the latitude (first value) and the
longitude (second value). All documents, which do not have the attribute
path or with value that are not suitable, are ignored.<br>
If it is an array with two attribute paths *latitude* and *longitude*,
then a geo-spatial index on all documents is created using *latitude*
and *longitude* as paths the latitude and the longitude. The value of
the attribute *latitude* and of the attribute *longitude* must a
double. All documents, which do not have the attribute paths or which
values are not suitable, are ignored.

@RESTBODYPARAM{geoJson,string,required,string}
If a geo-spatial index on a *location* is constructed
and *geoJson* is *true*, then the order within the array is longitude
followed by latitude. This corresponds to the format described in
http://geojson.org/geojson-spec.html#positions

@RESTDESCRIPTION
Creates a geo-spatial index in the collection *collection-name*, if
it does not already exist. Expects an object containing the index details.

Geo indexes are always sparse, meaning that documents that do not contain
the index attributes or have non-numeric values in the index attributes
will not be indexed.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the index already exists, then a *HTTP 200* is returned.

@RESTRETURNCODE{201}
If the index does not already exist and could be created, then a *HTTP 201*
is returned.

@RESTRETURNCODE{404}
If the *collection-name* is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Creating a geo index with a location attribute

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateGeoLocation}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = {
      type: "geo",
      fields : [ "b" ]
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN

Creating a geo index with latitude and longitude attributes

@EXAMPLE_ARANGOSH_RUN{RestIndexCreateGeoLatitudeLongitude}
    var cn = "products";
    db._drop(cn);
    db._create(cn);

    var url = "/_api/index?collection=" + cn;
    var body = {
      type: "geo",
      fields: [ "e", "f" ]
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
  ~ db._drop(cn);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
