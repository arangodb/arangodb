Write-ahead log
===============

`const wal = require('internal').wal`

This module provides functionality for administering the write-ahead logs.
Most of these functions only return sensible values when invoked with the 
[mmfiles engine being active](../../Administration/Configuration/GeneralArangod.md#storage-engine).

### Configuration
<!-- arangod/V8Server/v8-vocbase.h -->


retrieves the configuration of the write-ahead log
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


**Examples**


    @startDocuBlockInline WalPropertiesGet
    @EXAMPLE_ARANGOSH_OUTPUT{WalPropertiesGet}
      require("internal").wal.properties();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock WalPropertiesGet


<!-- arangod/V8Server/v8-vocbase.h -->


configures the write-ahead log
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


**Examples**


    @startDocuBlockInline WalPropertiesSet
    @EXAMPLE_ARANGOSH_OUTPUT{WalPropertiesSet}
    | require("internal").wal.properties({ 
    |    allowOverSizeEntries: true,
        logfileSize: 32 * 1024 * 1024 });
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock WalPropertiesSet


### Flushing

<!-- arangod/V8Server/v8-vocbase.h -->


flushes the currently open WAL logfile
`internal.wal.flush(waitForSync, waitForCollector)`

Flushes the write-ahead log. By flushing the currently active write-ahead
logfile, the data in it can be transferred to collection journals and
datafiles. This is useful to ensure that all data for a collection is
present in the collection journals and datafiles, for example, when dumping
the data of a collection.

The *waitForSync* option determines whether or not the operation should
block until the not-yet synchronized data in the write-ahead log was
synchronized to disk.

The *waitForCollector* operation can be used to specify that the operation
should block until the data in the flushed log has been collected by the
write-ahead log garbage collector. Note that setting this option to *true*
might block for a long time if there are long-running transactions and
the write-ahead log garbage collector cannot finish garbage collection.


**Examples**


    @startDocuBlockInline WalFlush
    @EXAMPLE_ARANGOSH_OUTPUT{WalFlush}
      require("internal").wal.flush();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock WalFlush


