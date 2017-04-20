

@brief counts the number of documents in a result set
`collection.count()`

Returns the number of living documents in the collection.

**Note**: this result may be slightly off when using the RocksDB storage engine.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{collectionCount}
~ db._create("users");
  db.users.count();
~ db._drop("users");
@END_EXAMPLE_ARANGOSH_OUTPUT
