

@brief drops an index
`db._dropIndex(index)`

Drops the *index*.  If the index does not exist, then *false* is
returned. If the index existed and was dropped, then *true* is
returned.

`db._dropIndex(index-id)`

Drops the index with *index-id*.

@EXAMPLE_ARANGOSH_OUTPUT{dropIndex}
~db._create("example");
db.example.ensureIndex({ type: "skiplist", fields: [ "a", "b" ] });
var indexInfo = db.example.getIndexes();
indexInfo;
db._dropIndex(indexInfo[0])
db._dropIndex(indexInfo[1].id)
indexInfo = db.example.getIndexes();
~db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

