

@brief drops an index
`collection.dropIndex(index)`

Drops the index. If the index does not exist, then *false* is
returned. If the index existed and was dropped, then *true* is
returned. Note that you cannot drop some special indexes (e.g. the primary
index of a collection or the edge index of an edge collection).

`collection.dropIndex(index-id)`

Same as above. Instead of an index an index id can be given.

@EXAMPLE_ARANGOSH_OUTPUT{col_dropIndex}
~db._create("example");
db.example.ensureSkiplist("a", "b");
var indexInfo = db.example.getIndexes();
indexInfo;
db.example.dropIndex(indexInfo[0])
db.example.dropIndex(indexInfo[1].id)
indexInfo = db.example.getIndexes();
~db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT


