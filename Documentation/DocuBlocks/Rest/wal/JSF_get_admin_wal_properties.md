
@startDocuBlock JSF_get_admin_wal_properties
@brief fetch the current configuration.

@RESTHEADER{GET /_admin/wal/properties, Retrieves the configuration of the write-ahead log}

@RESTDESCRIPTION

Retrieves the configuration of the write-ahead log. The result is a JSON
object with the following attributes:
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

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the operation succeeds.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.
@endDocuBlock

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestWalPropertiesGet}
    var url = "/_admin/wal/properties";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

