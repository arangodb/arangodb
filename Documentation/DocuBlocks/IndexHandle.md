

@brief finds an index
`db._index(index-id)`

Returns the index with *index-id* or null if no such index exists.

@EXAMPLE_ARANGOSH_OUTPUT{IndexHandle}
~db._create("example");
db.example.ensureIndex({ type: "skiplist", fields: [ "a", "b" ] });
var indexInfo = db.example.getIndexes().map(function(x) { return x.id; });
indexInfo;
db._index(indexInfo[0])
db._index(indexInfo[1])
~db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

