

@brief retrieves the configuration of the write-ahead log
`internal.wal.properties()`

Retrieves the configuration of the write-ahead log. The result is a JSON
array with the following attributes:
- *allowOversizeEntries*: whether or not operations that are bigger than a
  single logfile can be executed and stored
- *logfileSize*: the size of each write-ahead logfile
- *historicLogfiles*: the maximum number of historic logfiles to keep
- *reserveLogfiles*: the maximum number of reserve logfiles that ArangoDB
  allocates in the background
- *syncInterval*: the interval for automatic synchronization of not-yet
  synchronized write-ahead log data (in milliseconds)
- *throttleWait*: the maximum wait time that operations will wait before
  they get aborted if case of write-throttling (in milliseconds)
- *throttleWhenPending*: the number of unprocessed garbage-collection
  operations that, when reached, will activate write-throttling. A value of
  *0* means that write-throttling will not be triggered.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{WalPropertiesGet_mmfiles}
  require("internal").wal.properties();
@END_EXAMPLE_ARANGOSH_OUTPUT

