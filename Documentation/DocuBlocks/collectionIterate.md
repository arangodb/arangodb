

@brief iterates over some elements of a collection
`collection.iterate(iterator, options)`

Iterates over some elements of the collection and apply the function
*iterator* to the elements. The function will be called with the
document as first argument and the current number (starting with 0)
as second argument.

*options* must be an object with the following attributes:

- *limit* (optional, default none): use at most *limit* documents.

- *probability* (optional, default all): a number between *0* and
  *1*. Documents are chosen with this probability.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{accessViaGeoIndex}
~db._create("example")
|for (i = -90;  i <= 90;  i += 10) {
|  for (j = -180;  j <= 180;  j += 10) {
|    db.example.save({ name : "Name/" + i + "/" + j,
|                      home : [ i, j ],
|                      work : [ -i, -j ] });
|  }
|}

 db.example.ensureIndex({ type: "geo", fields: [ "home" ] });
 |items = db.example.getIndexes().map(function(x) { return x.id; });
 db.example.index(items[1]);
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


