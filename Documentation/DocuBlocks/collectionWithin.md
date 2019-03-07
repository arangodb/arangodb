
@startDocuBlock collectionWithin
@brief constructs a within query for a collection
`collection.within(latitude, longitude, radius)`

This will find all documents within a given radius around the coordinate
(*latitude*, *longitude*). The returned array is sorted by distance,
beginning with the nearest document.

In order to use the *within* operator, a geo index must be defined for the
collection. This index also defines which attribute holds the coordinates
for the document.  If you have more then one geo-spatial index, you can use
the `geo` operator to select a particular index.


`collection.within(latitude, longitude, radius).distance()`

This will add an attribute `_distance` to all documents returned, which
contains the distance between the given point and the document in meters.

`collection.within(latitude, longitude, radius).distance(name)`

This will add an attribute *name* to all documents returned, which
contains the distance between the given point and the document in meters.

**Note**: this method is not yet supported by the RocksDB storage engine.

Note: the *within* simple query function is **deprecated** as of ArangoDB 2.6.
The function may be removed in future versions of ArangoDB. The preferred
way for retrieving documents from a collection using the within operator  is
to use the AQL *WITHIN* function in an AQL query as follows:

```
FOR doc IN WITHIN(@@collection, @latitude, @longitude, @radius, @distanceAttributeName)
    RETURN doc
```

@EXAMPLES

To find all documents within a radius of 2000 km use:

@EXAMPLE_ARANGOSH_OUTPUT{009_collectionWithin}
~ db._create("geo");
~ db.geo.ensureIndex({ type: "geo", fields: [ "loc" ] });
|~ for (var i = -90;  i <= 90;  i += 10) {
|  for (var j = -180; j <= 180; j += 10) {
      db.geo.save({ name : "Name/" + i + "/" + j, loc: [ i, j ] }); } }
  db.geo.within(0, 0, 2000 * 1000).distance().toArray();
~ db._drop("geo");
@END_EXAMPLE_ARANGOSH_OUTPUT

@endDocuBlock
