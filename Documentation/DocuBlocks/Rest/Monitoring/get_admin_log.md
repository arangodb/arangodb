@startDocuBlock get_admin_log
@brief returns the server logs

@RESTHEADER{GET /_admin/log, Read global logs from the server (deprecated), getLog}

@HINTS
{% hint 'warning' %}
This endpoint should no longer be used. It is deprecated from version 3.8.0 on.
Use `/_admin/log/entries` instead, which provides the same data in a more
intuitive and easier to process format.
{% endhint %}

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

@RESTQUERYPARAM{serverId,string,optional}
Returns all log entries of the specified server. All other query parameters 
remain valid. If no serverId is given, the asked server
will reply. This parameter is only meaningful on Coordinators.

@RESTDESCRIPTION
Returns fatal, error, warning or info log messages from the server's global log.
The result is a JSON object with the attributes described below.

This API can be turned off via the startup option `--log.api-enabled`. In case
the API is disabled, all requests will be responded to with HTTP 403. If the
API is enabled, accessing it requires admin privileges, or even superuser
privileges, depending on the value of the `--log.api-enabled` startup option.

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

@RESTRETURNCODE{403}
is returned if there are insufficient privileges to access the logs.

@endDocuBlock
