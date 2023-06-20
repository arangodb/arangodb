
@startDocuBlock post_api_index_geo

@RESTHEADER{POST /_api/index#geo, Create a geo-spatial index, createIndexGeo}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{collection,string,required}
The collection name.

@RESTBODYPARAM{type,string,required,string}
must be equal to `"geo"`.

@RESTBODYPARAM{name,string,optional,string}
An easy-to-remember name for the index to look it up or refer to it in index hints.
Index names are subject to the same character restrictions as collection names.
If omitted, a name is auto-generated so that it is unique with respect to the
collection, e.g. `idx_832910498`.

@RESTBODYPARAM{fields,array,required,string}
An array with one or two attribute paths.

If it is an array with one attribute path `location`, then a geo-spatial
index on all documents is created using `location` as path to the
coordinates. The value of the attribute must be an array with at least two
double values. The array must contain the latitude (first value) and the
longitude (second value). All documents, which do not have the attribute
path or with value that are not suitable, are ignored.

If it is an array with two attribute paths `latitude` and `longitude`,
then a geo-spatial index on all documents is created using `latitude`
and `longitude` as paths the latitude and the longitude. The values of
the `latitude` and `longitude` attributes must each be a number (double).
All documents which do not have the attribute paths or which have
values that are not suitable are ignored.

@RESTBODYPARAM{geoJson,boolean,optional,}
If a geo-spatial index on a `location` is constructed
and `geoJson` is `true`, then the order within the array is longitude
followed by latitude. This corresponds to the format described in
http://geojson.org/geojson-spec.html#positions

@RESTBODYPARAM{legacyPolygons,boolean,optional,}
If `geoJson` is set to `true`, then this option controls how GeoJSON Polygons
are interpreted.

- If `legacyPolygons` is `true`, the smaller of the two regions defined by a
  linear ring is interpreted as the interior of the ring and a ring can at most
  enclose half the Earth's surface.
- If `legacyPolygons` is `false`, the area to the left of the boundary ring's
  path is considered to be the interior and a ring can enclose the entire
  surface of the Earth.

The default is `true` for geo indexes that were created in versions before 3.10,
and `false` for geo indexes created in 3.10 or later.

@RESTBODYPARAM{inBackground,boolean,optional,}
You can set this option to `true` to create the index
in the background, which will not write-lock the underlying collection for
as long as if the index is built in the foreground. The default value is `false`.

@RESTDESCRIPTION
Creates a geo-spatial index in the collection `collection`, if
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
If the `collection` is unknown, then a *HTTP 404* is returned.

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
