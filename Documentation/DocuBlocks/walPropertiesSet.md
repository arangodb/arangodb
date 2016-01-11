

@brief configures the write-ahead log
`internal.wal.properties(properties)`

Configures the behavior of the write-ahead log. *properties* must be a JSON
JSON object with the following attributes:
- *allowOversizeEntries*: whether or not operations that are bigger than a
  single logfile can be executed and stored
- *logfileSize*: the size of each write-ahead logfile
- *historicLogfiles*: the maximum number of historic logfiles to keep
- *reserveLogfiles*: the maximum number of reserve logfiles that ArangoDB
  allocates in the background
- *throttleWait*: the maximum wait time that operations will wait before
  they get aborted if case of write-throttling (in milliseconds)
- *throttleWhenPending*: the number of unprocessed garbage-collection
  operations that, when reached, will activate write-throttling. A value of
  *0* means that write-throttling will not be triggered.

Specifying any of the above attributes is optional. Not specified attributes
will be ignored and the configuration for them will not be modified.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{WalPropertiesSet}
| require("internal").wal.properties({ 
|    allowOverSizeEntries: true,
    logfileSize: 32 * 1024 * 1024 });
@END_EXAMPLE_ARANGOSH_OUTPUT

