
@startDocuBlock get_api_replication_logger_tick_ranges
@brief returns the tick value ranges available in the logfiles

@RESTHEADER{GET /_api/replication/logger-tick-ranges, Return the tick ranges available in the WAL logfiles,handleCommandLoggerTickRanges}

@RESTDESCRIPTION
Returns the currently available ranges of tick values for all currently
available WAL logfiles. The tick values can be used to determine if certain
data (identified by tick value) are still available for replication.

The body of the response contains a JSON array. Each array member is an
object
that describes a single logfile. Each object has the following attributes:

* *datafile*: name of the logfile

* *status*: status of the datafile, in textual form (e.g. "sealed", "open")

* *tickMin*: minimum tick value contained in logfile

* *tickMax*: maximum tick value contained in logfile

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the tick ranges could be determined successfully.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if the logger state could not be determined.

@RESTRETURNCODE{501}
is returned when this operation is called on a Coordinator in a cluster.

@EXAMPLES

Returns the available tick ranges.

@EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerTickRanges}
    var url = "/_api/replication/logger-tick-ranges";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
