
@startDocuBlock collectionGeo
@brief constructs a geo index selection
`collection.geo(location-attribute)`

Looks up a geo index defined on attribute *location_attribute*.

Returns a geo index object if an index was found. The `near` or
`within` operators can then be used to execute a geo-spatial query on
this particular index.

This is useful for collections with multiple defined geo indexes.

`collection.geo(location_attribute, true)`

Looks up a geo index on a compound attribute *location_attribute*.

Returns a geo index object if an index was found. The `near` or
`within` operators can then be used to execute a geo-spatial query on
this particular index.

`collection.geo(latitude_attribute, longitude_attribute)`

Looks up a geo index defined on the two attributes *latitude_attribute*
and *longitude-attribute*.

Returns a geo index object if an index was found. The `near` or
`within` operators can then be used to execute a geo-spatial query on
this particular index.

**Note**: this method is not yet supported by the RocksDB storage engine.

Note: the *geo* simple query helper function is **deprecated** as of ArangoDB
2.6. The function may be removed in future versions of ArangoDB. The preferred
way for running geo queries is to use their AQL equivalents.

@EXAMPLES

Assume you have a location stored as list in the attribute *home*
and a destination stored in the attribute *work*. Then you can use the
`geo` operator to select which geo-spatial attributes (and thus which
index) to use in a `near` query.

@EXAMPLE_ARANGOSH_OUTPUT{geoIndexSimpleQuery}
~db._create("complex")
|for (i = -90;  i <= 90;  i += 10) {
|  for (j = -180;  j <= 180;  j += 10) {
|    db.complex.save({ name : "Name/" + i + "/" + j,
|                      home : [ i, j ],
|                      work : [ -i, -j ] });
|  }
|}

 db.complex.near(0, 170).limit(5); // xpError(ERROR_QUERY_GEO_INDEX_MISSING)
 db.complex.ensureIndex({ type: "geo", fields: [ "home" ] });
 db.complex.near(0, 170).limit(5).toArray();
 db.complex.geo("work").near(0, 170).limit(5); // xpError(ERROR_QUERY_GEO_INDEX_MISSING)
 db.complex.ensureIndex({ type: "geo", fields: [ "work" ] });
 db.complex.geo("work").near(0, 170).limit(5).toArray();
~ db._drop("complex");
@END_EXAMPLE_ARANGOSH_OUTPUT


@endDocuBlock
