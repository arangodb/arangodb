
@startDocuBlock JSF_get_admin_log
@brief returns the server logs

@RESTHEADER{GET /_admin/log, Read global logs from the server}

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

- *lid*: a list of log entry identifiers. Each log message is uniquely
  identified by its @LIT{lid} and the identifiers are in ascending
  order.

- *level*: a list of the log-levels for all log entries.

- *timestamp*: a list of the timestamps as seconds since 1970-01-01 for all log
  entries.

- *text* a list of the texts of all log entries

- *totalAmount*: the total amount of log entries before pagination.

@RESTRETURNCODES

@RESTRETURNCODE{400}
is returned if invalid values are specified for *upto* or *level*.

@RESTRETURNCODE{500}
is returned if the server cannot generate the result due to an out-of-memory
error.
@endDocuBlock


@startDocuBlock JSF_get_admin_loglevel
@brief returns the current loglevel settings

@RESTHEADER{GET /_admin/log/level, Return the current server loglevel}

@RESTDESCRIPTION
Returns the server's current loglevel settings.
The result is a JSON object with the log topics being the object keys, and
the log levels being the object values.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request is valid

@RESTRETURNCODE{500}
is returned if the server cannot generate the result due to an out-of-memory
error.
@endDocuBlock


@startDocuBlock JSF_put_admin_loglevel
@brief modifies the current loglevel settings

@RESTHEADER{PUT /_admin/log/level, Modify and return the current server loglevel}

@RESTDESCRIPTION
Modifies and returns the server's current loglevel settings.
The request body must be a JSON object with the log topics being the object keys
and the log levels being the object values.

The result is a JSON object with the adjusted log topics being the object keys, and
the adjusted log levels being the object values.

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

