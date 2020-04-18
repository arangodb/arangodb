

@brief returns the figures of a collection
`collection.figures()`

Returns an object containing statistics about the collection.

* *indexes.count*: The total number of indexes defined for the
* *indexes.size*: The total memory allocated for indexes in bytes.
<!-- TODO: describe RocksDB figures -->
* *documentsSize*
* *cacheInUse*
* *cacheSize*
* *cacheUsage*

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionFigures}
~ require("internal").wal.flush(true, true);
  db.demo.figures()
@END_EXAMPLE_ARANGOSH_OUTPUT
