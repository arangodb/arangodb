Geo-Spatial Indexes
===================

ArangoDB features a [Google S2](http://s2geometry.io/) based geospatial index.
We support indexing on a subset of the [**GeoJSON**](#geojson) standard
(as well as simple latitude longitude pairs).

AQL's geospatial functions and GeoJSON constructors are described in
[Geo functions](../../AQL/Functions/Geo.html).

Using a Geo-Spatial Index
-------------------------

The geospatial index supports containment and intersection
queries for various geometric 2D shapes. You should be mainly using AQL queries
to perform these types of operations. The index can operate in **two different
modes**, depending on if you want to use the GeoJSON data-format or not. The modes
are mainly toggled by using the `geoJson` field when creating the index.

This index assumes coordinates with the latitude between -90 and 90 degrees and the
longitude between -180 and 180 degrees. A geo index will ignore all 
documents which do not fulfill these requirements.

### GeoJSON Mode

To create an index in GeoJSON mode execute:

```
collection.ensureIndex({ type: "geo", fields: [ "geometry" ], geoJson:true })
```

This creates the index on all documents and uses _geometry_ as the attributed
field where the value is either a [Geometry
Object](https://tools.ietf.org/html/rfc7946#section-3.1) **or** a _coordinate
array_. The array must contain at least two numeric values with longitude (first
value) and the latitude (second value). This  corresponds to the format
described in [RFC 7946
Position](https://tools.ietf.org/html/rfc7946#section-3.1.1)

All documents, which do not have the attribute path or have a non-conform
value in it, are excluded from the index.

A geo index is implicitly sparse, and there is no way to control its sparsity.
In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.


### Non-GeoJSON mode

This index mode exclusively supports indexing on coordinate arrays. Values that
contain GeoJSON or other types of data will be ignored. In the non-GeoJSON mode
the index can be created on one or two fields.

The following examples will work in the _arangosh_ command shell.

To create a geo-spatial index on all documents using *latitude* and
*longitude* as separate attribute paths, two paths need to be specified
in the *fields* array:

`collection.ensureIndex({ type: "geo", fields: [ "latitude", "longitude" ] })`

The first field is always defined to be the _latitude_ and the second is the
_longitude_. The `geoJson` flag is implicitly _false_ in this mode.

Alternatively you can specify only one field:

`collection.ensureIndex({ type: "geo", fields: [ "location" ], geoJson:false })`

It creates a geospatial index on all documents using *location* as the path to the
coordinates. The value of the attribute has to be an array with at least two
numeric values. The array must contain the latitude (first value) and the
longitude (second value).

All documents, which do not have the attribute path(s) or have a non-conforming
value in it, are excluded from the index.

A geo index is implicitly sparse, and there is no way to control its sparsity.
In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

Indexed GeoSpatial Queries
--------------------------

The geospatial index supports a variety of AQL queries, which can be built with the help
of the [geo utility functions](../../AQL/Functions/Geo.html). There are three specific
geo functions that can be optimized, provided that they are used correctly:
`GEO_DISTANCE, GEO_CONTAINS, GEO_INTERSECTS`. Additionally, there is a built-in support to optimize
the older geo functions `DISTANCE`, `NEAR` and `WITHIN` (the last two only if they are
used in their 4 argument version, without *distanceName*).

When in doubt whether your query is being properly optimized, 
check the [AQL explain](../../AQL/ExecutionAndPerformance/ExplainingQueries.html)
output to check for index usage.

### Query for Results near Origin (NEAR type query)

A basic example of a query for results near an origin point:

```
FOR x IN geo_collection
  FILTER GEO_DISTANCE([@lng, @lat], x.geometry) <= 100000
  RETURN x._key
```
The first parameter can be a GeoJSON object or a coordinate array in `[longitude, latitude]` ordering.
The second parameter is the document field on which the index was created. The function
`GEO_DISTANCE` always returns the distance in meters, so will receive results
up until _100km_.


### Query for Sorted Results near Origin (NEAR type query)

A basic example of a query for the 1000 nearest results to an origin point (ascending sorting):

```
FOR x IN geo_collection
  SORT GEO_DISTANCE([@lng, @lat], x.geometry) ASC
  LIMIT 1000
  RETURN x._key
```

The first parameter can be a GeoJSON object or a coordinate array in `[longitude, latitude]` ordering.
The second parameter is the documents field on which the index was created.

You may also get results farthest away (distance sorted in descending order):

```
FOR x IN geo_collection
  SORT GEO_DISTANCE([@lng, @lat], x.geometry) DESC
  LIMIT 1000
  RETURN x._key
```

### Query for Results within Distance

A query which returns documents at a distance of _1km_ or farther away,
up to _100km_ from the origin. This will return the documents with a GeoJSON
value that is located in the specified search annulus.

```
FOR x IN geo_collection
  FILTER GEO_DISTANCE([@lng, @lat], x.geometry) <= 100000
  FILTER GEO_DISTANCE([@lng, @lat], x.geometry) >= 1000
  RETURN x
```

### Query for Results contained in Polygon

A query which returns documents whose stored geometry is contained within a
GeoJSON Polygon.

```
LET polygon = GEO_POLYGON([[[60,35],[50,5],[75,10],[70,35]]])
FOR x IN geo_collection
  FILTER GEO_CONTAINS(polygon, x.geometry)
  RETURN x
```

The first parameter of `GEO_CONTAINS` must be a polygon. Other types are not valid. 
The second parameter must contain the document field on which the index was created.


### Query for Results Intersecting a Polygon

A query which returns documents with an intersection of their stored geometry and a
GeoJSON Polygon.

```
LET polygon = GEO_POLYGON([[[60,35],[50,5],[75,10],[70,35]]])
FOR x IN geo_collection
  FILTER GEO_INTERSECTS(polygon, x.geometry)
  RETURN x
```

The first parameter of `GEO_INTERSECTS` must be a polygon. Other types are not valid. 
The second parameter must contain the document field on which the index was created.


GeoJSON
-------

GeoJSON is a geospatial data format based on JSON. It defines several different
types of JSON objects and the way in which they can be combined to represent
data about geographic shapes on the earth surface. GeoJSON uses a geographic
coordinate reference system, World Geodetic System 1984 (WGS 84), and units of decimal
degrees.

Internally ArangoDB maps all coordinates onto a unit sphere. Distances are
projected onto a sphere with the Earth's *Volumetric mean radius* of *6371
km*. ArangoDB implements a useful subset of the GeoJSON format [(RFC
7946)](https://tools.ietf.org/html/rfc7946). We do not support Feature Objects
or the GeometryCollection type. Supported geometry object types are:

- Point
- MultiPoint
- LineString
- MultiLineString
- Polygon

### Point

A [GeoJSON Point](https://tools.ietf.org/html/rfc7946#section-3.1.2) is a
[position](https://tools.ietf.org/html/rfc7946#section-3.1.1) comprised of
a longitude and a latitude:

```json
{
  "type": "Point",
  "coordinates": [100.0, 0.0]
}
```

### MultiPoint

A [GeoJSON MultiPoint](https://tools.ietf.org/html/rfc7946#section-3.1.7) is
an array of positions:

```json
{
  "type": "MultiPoint",
  "coordinates": [
    [100.0, 0.0],
    [101.0, 1.0]
  ]
}
```

### LineString

A [GeoJSON LineString](https://tools.ietf.org/html/rfc7946#section-3.1.4) is
an array of two or more positions:

```json
{
  "type": "LineString",
  "coordinates": [
    [100.0, 0.0],
    [101.0, 1.0]
  ]
}
```

### MultiLineString

A [GeoJSON MultiLineString](https://tools.ietf.org/html/rfc7946#section-3.1.5) is
an array of LineString coordinate arrays:

```json
{
  "type": "MultiLineString",
  "coordinates": [
    [
      [100.0, 0.0],
      [101.0, 1.0]
    ],
    [
      [102.0, 2.0],
      [103.0, 3.0]
    ]
  ]
}
```

### Polygon

A [GeoJSON Polygon](https://tools.ietf.org/html/rfc7946#section-3.1.6) consists
of a series of closed `LineString` objects (ring-like). These *Linear Ring* objects
consist of four or more vertices with the first and last coordinate pairs
being equal. Coordinates of a Polygon are an array of linear ring coordinate
arrays. The first element in the array represents the exterior ring.
Any subsequent elements represent interior rings (holes within the surface).

- A linear ring may not be empty, it needs at least three _distinct_ coordinates
- Within the same linear ring consecutive coordinates may be the same, otherwise
  (except the first and last one) all coordinates need to be distinct

No Holes:

```json
{
  "type": "Polygon",
    "coordinates": [
    [
      [100.0, 0.0],
      [101.0, 0.0],
      [101.0, 1.0],
      [100.0, 1.0],
      [100.0, 0.0]
    ]
  ]
}
```

With Holes:

- The exterior ring should not self-intersect.
- The interior rings must be contained in the outer ring
- No two rings can cross each other, i.e. no ring may intersect both the interior and exterior face of another ring
- Rings cannot share edges, they may however share vertices
- No ring may be empty
- Polygon rings should follow the right-hand rule for orientation
  (counterclockwise external rings, clockwise internal rings).

```json
{
  "type": "Polygon",
  "coordinates": [
    [
      [100.0, 0.0],
      [101.0, 0.0],
      [101.0, 1.0],
      [100.0, 1.0],
      [100.0, 0.0]
    ],
    [
      [100.8, 0.8],
      [100.8, 0.2],
      [100.2, 0.2],
      [100.2, 0.8],
      [100.8, 0.8]
    ]
  ]
}
```

### MultiPolygon

A [GeoJSON MultiPolygon](https://tools.ietf.org/html/rfc7946#section-3.1.6) consists
of multiple polygons. The "coordinates" member is an array of
_Polygon_ coordinate arrays. 

- Polygons in the same MultiPolygon may not share edges, they may share coordinates
- Polygons and rings must not be empty
- Linear Rings **MUST** follow the right-hand rule for orientation
  (counterclockwise external rings, clockwise internal rings).

Example with two polygons, the second one with a hole:

```json
{
    "type": "MultiPolygon",
    "coordinates": [
        [
            [
                [102.0, 2.0],
                [103.0, 2.0],
                [103.0, 3.0],
                [102.0, 3.0],
                [102.0, 2.0]
            ]
        ],
        [
            [
                [100.0, 0.0],
                [101.0, 0.0],
                [101.0, 1.0],
                [100.0, 1.0],
                [100.0, 0.0]
            ],
            [
                [100.2, 0.2],
                [100.2, 0.8],
                [100.8, 0.8],
                [100.8, 0.2],
                [100.2, 0.2]
            ]
        ]
    ]
}
```

Arangosh Examples
-----------------

<!-- js/server/modules/@arangodb/arango-collection.js-->

ensures that a geo index exists
`collection.ensureIndex({ type: "geo", fields: [ "location" ] })`

Creates a geospatial index on all documents using *location* as the path to the
coordinates. The value of the attribute has to be an array with at least two
numeric values. The array must contain the latitude (first value) and the
longitude (second value).

All documents, which do not have the attribute path or have a non-conforming
value in it, are excluded from the index.

A geo index is implicitly sparse, and there is no way to control its sparsity.

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

To create a geo index on an array attribute that contains longitude first, set
the *geoJson* attribute to `true`. This corresponds to the format described in
[RFC 7946 Position](https://tools.ietf.org/html/rfc7946#section-3.1.1)

`collection.ensureIndex({ type: "geo", fields: [ "location" ], geoJson: true })`

To create a geo-spatial index on all documents using *latitude* and *longitude*
as separate attribute paths, two paths need to be specified in the *fields*
array:

`collection.ensureIndex({ type: "geo", fields: [ "latitude", "longitude" ] })`

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

**Examples**

Create a geo index for an array attribute:

    @startDocuBlockInline geoIndexCreateForArrayAttribute1
    @EXAMPLE_ARANGOSH_OUTPUT{geoIndexCreateForArrayAttribute1}
    ~db._create("geo")
     db.geo.ensureIndex({ type: "geo", fields: [ "loc" ] });
    | for (i = -90;  i <= 90;  i += 10) {
    |     for (j = -180; j <= 180; j += 10) {
    |         db.geo.save({ name : "Name/" + i + "/" + j, loc: [ i, j ] });
    |     }
      }
    db.geo.count();
    db.geo.near(0, 0).limit(3).toArray();
    db.geo.near(0, 0).count();
    ~db._drop("geo")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock geoIndexCreateForArrayAttribute1

Create a geo index for a hash array attribute:

    @startDocuBlockInline geoIndexCreateForArrayAttribute2
    @EXAMPLE_ARANGOSH_OUTPUT{geoIndexCreateForArrayAttribute2}
    ~db._drop("geo2")
    ~db._create("geo2")
    db.geo2.ensureIndex({ type: "geo", fields: [ "location.latitude", "location.longitude" ] });
    | for (i = -90;  i <= 90;  i += 10) {
    |     for (j = -180; j <= 180; j += 10) {
    |         db.geo2.save({ name : "Name/" + i + "/" + j, location: { latitude : i, longitude : j } });
    |     }
      }
    db.geo2.near(0, 0).limit(3).toArray();
    ~db._drop("geo2")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock geoIndexCreateForArrayAttribute2

Use GeoIndex with AQL SORT statement:

    @startDocuBlockInline geoIndexSortOptimization
    @EXAMPLE_ARANGOSH_OUTPUT{geoIndexSortOptimization}
    ~db._create("geoSort")
    db.geoSort.ensureIndex({ type: "geo", fields: [ "latitude", "longitude" ] });
    | for (i = -90;  i <= 90;  i += 10) {
    |     for (j = -180; j <= 180; j += 10) {
    |         db.geoSort.save({ name : "Name/" + i + "/" + j, latitude : i, longitude : j });
    |     }
      }
    var query = "FOR doc in geoSort SORT DISTANCE(doc.latitude, doc.longitude, 0, 0) LIMIT 5 RETURN doc"
    db._explain(query, {}, {colors: false});
    db._query(query);
    ~db._drop("geoSort")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock geoIndexSortOptimization

Use GeoIndex with AQL FILTER statement:

    @startDocuBlockInline geoIndexFilterOptimization
    @EXAMPLE_ARANGOSH_OUTPUT{geoIndexFilterOptimization}
    ~db._create("geoFilter")
    db.geoFilter.ensureIndex({ type: "geo", fields: [ "latitude", "longitude" ] });
    | for (i = -90;  i <= 90;  i += 10) {
    |     for (j = -180; j <= 180; j += 10) {
    |         db.geoFilter.save({ name : "Name/" + i + "/" + j, latitude : i, longitude : j });
    |     }
      }
    var query = "FOR doc in geoFilter FILTER DISTANCE(doc.latitude, doc.longitude, 0, 0) < 2000 RETURN doc"
    db._explain(query, {}, {colors: false});
    db._query(query);
    ~db._drop("geoFilter")
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock geoIndexFilterOptimization


<!-- js/common/modules/@arangodb/arango-collection-common.js-->
@startDocuBlock collectionGeo

<!-- js/common/modules/@arangodb/arango-collection-common.js-->
@startDocuBlock collectionNear

<!-- js/common/modules/@arangodb/arango-collection-common.js-->
@startDocuBlock collectionWithin

ensures that a geo index exists
`collection.ensureIndex({ type: "geo", fields: [ "location" ] })`

Since ArangoDB 2.5, this method is an alias for *ensureGeoIndex* since
geo indexes are always sparse, meaning that documents that do not contain
the index attributes or has non-numeric values in the index attributes
will not be indexed. *ensureGeoConstraint* is deprecated and *ensureGeoIndex*
should be used instead.

The index does not provide a `unique` option because of its limited usability.
It would prevent identical coordinates from being inserted only, but even a
slightly different location (like 1 inch or 1 cm off) would be unique again and
not considered a duplicate, although it probably should. The desired threshold
for detecting duplicates may vary for every project (including how to calculate
the distance even) and needs to be implemented on the application layer as
needed. You can write a [Foxx service](../Foxx/index.html) for this purpose and
make use of the AQL [geo functions](../../AQL/Functions/Geo.html) to find nearby
coordinates supported by a geo index.
