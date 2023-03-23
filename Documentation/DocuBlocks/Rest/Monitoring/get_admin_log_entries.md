@startDocuBlock get_admin_log_entries
@brief returns the server logs

@RESTHEADER{GET /_admin/log/entries, Read global logs from the server, getLogEntries}

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
Returns all log entries such that their log entry identifier (*lid* .)
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
(if *sort* is *desc*) according to their *id* values. Note that the *id*
imposes a chronological order. The default value is *asc*.

@RESTQUERYPARAM{serverId,string,optional}
Returns all log entries of the specified server. All other query parameters 
remain valid. If no serverId is given, the asked server
will reply. This parameter is only meaningful on Coordinators.

@RESTDESCRIPTION
Returns fatal, error, warning or info log messages from the server's global log.
The result is a JSON object with the following properties:

- **total**: the total amount of log entries before pagination 
- **messages**: an array with log messages that matched the criteria

This API can be turned off via the startup option `--log.api-enabled`. In case
the API is disabled, all requests will be responded to with HTTP 403. If the
API is enabled, accessing it requires admin privileges, or even superuser
privileges, depending on the value of the `--log.api-enabled` startup option.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request is valid.

@RESTRETURNCODE{400}
is returned if invalid values are specified for *upto* or *level*.

@RESTRETURNCODE{403}
is returned if there are insufficient privileges to access the logs.

@endDocuBlock
