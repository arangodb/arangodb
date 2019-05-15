
@startDocuBlock put_admin_wal_properties
@brief configure parameters of the wal

@RESTHEADER{PUT /_admin/wal/properties, Configures the write-ahead log, RestWalHandler:properties}

@RESTDESCRIPTION
Configures the behavior of the write-ahead log. The body of the request
must be a JSON object with the following attributes:
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

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the operation succeeds.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.
@endDocuBlock

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestWalPropertiesPut}
    var url = "/_admin/wal/properties";
    var body = {
      logfileSize: 32 * 1024 * 1024,
      allowOversizeEntries: true
    };
    var response = logCurlRequest('PUT', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

