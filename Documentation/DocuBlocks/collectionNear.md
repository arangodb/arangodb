
@startDocuBlock collectionNear
@brief constructs a near query for a collection
`collection.near(latitude, longitude)`

The returned list is sorted according to the distance, with the nearest
document to the coordinate (*latitude*, *longitude*) coming first.
If there are near documents of equal distance, documents are chosen randomly
from this set until the limit is reached. It is possible to change the limit
using the *limit* operator.

In order to use the *near* operator, a geo index must be defined for the
collection. This index also defines which attribute holds the coordinates
for the document.  If you have more then one geo-spatial index, you can use
the *geo* operator to select a particular index.

*Note*: `near` does not support negative skips.
//     However, you can still use `limit` followed to skip.

`collection.near(latitude, longitude).limit(limit)`

Limits the result to limit documents instead of the default 100.

*Note*: Unlike with multiple explicit limits, `limit` will raise
the implicit default limit imposed by `within`.

`collection.near(latitude, longitude).distance()`

This will add an attribute `distance` to all documents returned, which
contains the distance between the given point and the document in meters.

`collection.near(latitude, longitude).distance(name)`

This will add an attribute *name* to all documents returned, which
contains the distance between the given point and the document in meters.

**Note**: this method is not yet supported by the RocksDB storage engine.

Note: the *near* simple query function is **deprecated** as of ArangoDB 2.6.
The function may be removed in future versions of ArangoDB. The preferred
way for retrieving documents from a collection using the near operator is
to use the AQL *NEAR* function in an AQL query as follows:

```js
FOR doc IN NEAR(@@collection, @latitude, @longitude, @limit)
    RETURN doc
```

@EXAMPLES

To get the nearest two locations:

@EXAMPLE_ARANGOSH_OUTPUT{007_collectionNear}
~ db._drop("geo");
~ db._create("geo");
  db.geo.ensureIndex({ type: "geo", fields: [ "loc" ] });
  |for (var i = -90;  i <= 90;  i += 10) {
  |   for (var j = -180; j <= 180; j += 10) {
  |     db.geo.save({
  |        name : "Name/" + i + "/" + j,
  |        loc: [ i, j ] });
  } }
  db.geo.near(0, 0).limit(2).toArray();
~ db._drop("geo");
@END_EXAMPLE_ARANGOSH_OUTPUT

If you need the distance as well, then you can use the `distance`
operator:

@EXAMPLE_ARANGOSH_OUTPUT{008_collectionNearDistance}
~ db._create("geo");
  db.geo.ensureIndex({ type: "geo", fields: [ "loc" ] });
  |for (var i = -90;  i <= 90;  i += 10) {
  |  for (var j = -180; j <= 180; j += 10) {
  |     db.geo.save({
  |         name : "Name/" + i + "/" + j,
  |         loc: [ i, j ] });
  } }
  db.geo.near(0, 0).distance().limit(2).toArray();
~ db._drop("geo");
@END_EXAMPLE_ARANGOSH_OUTPUT

@endDocuBlock
