Geo Indexes
===========

### Introduction to Geo Indexes

This is an introduction to ArangoDB's geo indexes.

AQL's geographic features are described in [Geo functions](../../AQL/Functions/Geo.html).

ArangoDB uses Hilbert curves to implement geo-spatial indexes.
See this [blog](https://www.arangodb.com/2012/03/31/using-hilbert-curves-and-polyhedrons-for-geo-indexing)
for details.

A geo-spatial index assumes that the latitude is between -90 and 90 degree and
the longitude is between -180 and 180 degree. A geo index will ignore all
documents which do not fulfill these requirements.

Accessing Geo Indexes from the Shell
------------------------------------

<!-- js/server/modules/@arangodb/arango-collection.js-->


ensures that a geo index exists
`collection.ensureIndex({ type: "geo", fields: [ "location" ] })`

Creates a geo-spatial index on all documents using *location* as path to
the coordinates. The value of the attribute has to be an array with at least two
numeric values. The array must contain the latitude (first value) and the
longitude (second value).

All documents, which do not have the attribute path or have a non-conforming
value in it are excluded from the index.

A geo index is implicitly sparse, and there is no way to control its sparsity.

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

To create a geo index on an array attribute that contains longitude first, set the
*geoJson* attribute to `true`. This corresponds to the format described in
[RFC 7946 Position](https://tools.ietf.org/html/rfc7946#section-3.1.1)

`collection.ensureIndex({ type: "geo", fields: [ "location" ], geoJson: true })`

To create a geo-spatial index on all documents using *latitude* and
*longitude* as separate attribute paths, two paths need to be specified
in the *fields* array:

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
the index attributes or have non-numeric values in the index attributes
will not be indexed. *ensureGeoConstraint* is deprecated and *ensureGeoIndex*
should be used instead.

The index does not provide a `unique` option because of its limited usability.
It would prevent identical coordinates from being inserted only, but even a
slightly different location (like 1 inch or 1 cm off) would be unique again and
not considered a duplicate, although it probably should. The desired threshold
for detecting duplicates may vary for every project (including how to calculate
the distance even) and needs to be implemented on the application layer as needed.
You can write a [Foxx service](../Foxx/index.html) for this purpose and make use
of the AQL [geo functions](../../AQL/Functions/Geo.html) to find nearby
coordinates supported by a geo index.
