

@brief return the path to database files
`db._path()`

Returns the filesystem path of the current database as a string. When using the RocksDB storage engine, all databases for a server will share the same path.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{dbPath}
  require("@arangodb").db._path();
@END_EXAMPLE_ARANGOSH_OUTPUT
