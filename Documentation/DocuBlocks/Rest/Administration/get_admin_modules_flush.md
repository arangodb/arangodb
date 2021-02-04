
@startDocuBlock get_admin_log
@brief returns the server logs

@RESTHEADER{GET /_admin/log, Read global logs from the server, setLogLevel:read}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{upto,string,optional}
Returns all log entries up to log level *upto*. Note that *upto* must be:
- *fatal* or *0*
- *error* or *1*
- *warning* or *2*
- *info* or *3*
- *debug*  or *4*
The default value is *info*.

@RESTQUERYPARAM{level,string,optional}
Returns all log entries of log level *level*. Note that the query parameters
*upto* and *level* are mutually exclusive.

@RESTQUERYPARAM{start,number,optional}
Returns all log entries such that their log entry identifier (*lid* value)
is greater or equal to *start*.

@RESTQUERYPARAM{size,number,optional}
Restricts the result to at most *size* log entries.

@RESTQUERYPARAM{offset,number,optional}
Starts to return log entries skipping the first *offset* log entries. *offset*
and *size* can be used for pagination.

@RESTQUERYPARAM{search,string,optional}
Only return the log entries containing the text specified in *search*.

@RESTQUERYPARAM{sort,string,optional}
Sort the log entries either ascending (if *sort* is *asc*) or descending
(if *sort* is *desc*) according to their *lid* values. Note that the *lid*
imposes a chronological order. The default value is *asc*.

@RESTDESCRIPTION
Returns fatal, error, warning or info log messages from the server's global log.
The result is a JSON object with the following attributes:

@RESTRETURNCODES

@RESTRETURNCODE{200}

@RESTREPLYBODY{lid,array,required,string}
a list of log entry identifiers. Each log message is uniquely
identified by its @LIT{lid} and the identifiers are in ascending
order.

@RESTREPLYBODY{level,string,required,string}
A list of the log levels for all log entries.

@RESTREPLYBODY{timestamp,array,required,string}
a list of the timestamps as seconds since 1970-01-01 for all log
entries.

@RESTREPLYBODY{text,string,required,string}
a list of the texts of all log entries

@RESTREPLYBODY{topic,string,required,string}
a list of the topics of all log entries

@RESTREPLYBODY{totalAmount,integer,required,int64}
the total amount of log entries before pagination.

@RESTRETURNCODE{400}
is returned if invalid values are specified for *upto* or *level*.

@RESTRETURNCODE{500}
is returned if the server cannot generate the result due to an out-of-memory
error.
@endDocuBlock


@startDocuBlock get_admin_loglevel
@brief returns the current log level settings

@RESTHEADER{GET /_admin/log/level, Return the current server log level}

@RESTDESCRIPTION
Returns the server's current log level settings.
The result is a JSON object with the log topics being the object keys, and
the log levels being the object values.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request is valid

@RESTRETURNCODE{500}
is returned if the server cannot generate the result due to an out-of-memory
error.
@endDocuBlock


@startDocuBlock put_admin_loglevel
@brief modifies the current log level settings

@RESTHEADER{PUT /_admin/log/level, Modify and return the current server log level}

@RESTDESCRIPTION
Modifies and returns the server's current log level settings.
The request body must be a JSON object with the log topics being the object keys
and the log levels being the object values.

The result is a JSON object with the adjusted log topics being the object keys, and
the adjusted log levels being the object values.

It can set the log level of all facilities by only specifying the log level as string without json.

Possible log levels are:
- FATAL - There will be no way out of this. ArangoDB will go down after this message.
- ERROR - This is an error. you should investigate and fix it. It may harm your production.
- WARNING - This may be serious application-wise, but we don't know.
- INFO - Something has happened, take notice, but no drama attached.
- DEBUG - output debug messages
- TRACE - trace - prepare your log to be flooded - don't use in production.

@RESTBODYPARAM{agency,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{agencycomm,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{agencystore,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{aql,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{arangosearch,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{audit-authentication,string,optional,string}
One of the possible log levels (_Enterprise Edition only_).

@RESTBODYPARAM{audit-authorization,string,optional,string}
One of the possible log levels (_Enterprise Edition only_).

@RESTBODYPARAM{audit-collection,string,optional,string}
One of the possible log levels (_Enterprise Edition only_).

@RESTBODYPARAM{audit-database,string,optional,string}
One of the possible log levels (_Enterprise Edition only_).

@RESTBODYPARAM{audit-document,string,optional,string}
One of the possible log levels (_Enterprise Edition only_).

@RESTBODYPARAM{audit-hotbackup,string,optional,string}
One of the possible log levels (_Enterprise Edition only_).

@RESTBODYPARAM{audit-service,string,optional,string}
One of the possible log levels (_Enterprise Edition only_).

@RESTBODYPARAM{audit-view,string,optional,string}
One of the possible log levels (_Enterprise Edition only_).

@RESTBODYPARAM{authentication,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{authorization,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{backup,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{cache,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{cluster,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{clustercomm,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{collector,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{communication,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{compactor,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{config,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{crash,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{datafiles,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{development,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{dump,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{engines,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{flush,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{general,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{graphs,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{heartbeat,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{httpclient,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{ldap,string,optional,string}
One of the possible log levels (_Enterprise Edition only_).

@RESTBODYPARAM{maintenance,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{memory,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{mmap,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{performance,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{pregel,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{queries,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{replication,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{requests,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{restore,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{rocksdb,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{security,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{ssl,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{startup,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{statistics,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{supervision,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{syscall,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{threads,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{trx,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{ttl,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{validation,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{v8,string,optional,string}
One of the possible log levels.

@RESTBODYPARAM{views,string,optional,string}
One of the possible log levels.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request is valid

@RESTRETURNCODE{400}
is returned when the request body contains invalid JSON.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@RESTRETURNCODE{500}
is returned if the server cannot generate the result due to an out-of-memory
error.
@endDocuBlock
