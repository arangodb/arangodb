

@brief ensures that a geo index exists
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

To create a geo on an array attribute that contains longitude first, set the
*geoJson* attribute to `true`. This corresponds to the format described in
[positions](http://geojson.org/geojson-spec.html)

`collection.ensureIndex({ type: "geo", fields: [ "location" ], geoJson: true })`

To create a geo-spatial index on all documents using *latitude* and
*longitude* as separate attribute paths, two paths need to be specified
in the *fields* array:

`collection.ensureIndex({ type: "geo", fields: [ "latitude", "longitude" ] })`

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

**Note**: this method is not yet supported by the RocksDB storage engine.

@EXAMPLES

Create a geo index for an array attribute:

@EXAMPLE_ARANGOSH_OUTPUT{geoIndexCreateForArrayAttribute}
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

Create a geo index for a hash array attribute:

@EXAMPLE_ARANGOSH_OUTPUT{geoIndexCreateForArrayAttribute2}
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
