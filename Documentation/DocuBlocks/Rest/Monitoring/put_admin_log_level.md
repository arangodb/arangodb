@startDocuBlock put_admin_log_level

@RESTHEADER{PUT /_admin/log/level, Set the server log levels, setLogLevel}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{serverId,string,optional}
Forwards the request to the specified server.

@RESTDESCRIPTION
Modifies and returns the server's current log level settings.
The request body must be a JSON string with a log level or a JSON object with the 
log topics being the object keys and the log levels being the object values.

If only a JSON string is specified as input, the log level is adjusted for the 
"general" log topic only. If a JSON object is specified as input, the log levels will
be set only for the log topic mentioned in the input object, but preserved for every
other log topic.
To set the log level for all log levels to a specific value, it is possible to hand
in the special pseudo log topic "all".

The result is a JSON object with all available log topics being the object keys, and
the adjusted log levels being the object values.

Possible log levels are:
- FATAL - There will be no way out of this. ArangoDB will go down after this message.
- ERROR - This is an error. you should investigate and fix it. It may harm your production.
- WARNING - This may be serious application-wise, but we don't know.
- INFO - Something has happened, take notice, but no drama attached.
- DEBUG - output debug messages
- TRACE - trace - prepare your log to be flooded - don't use in production.

This API can be turned off via the startup option `--log.api-enabled`. In case
the API is disabled, all requests will be responded to with HTTP 403. If the
API is enabled, accessing it requires admin privileges, or even superuser
privileges, depending on the value of the `--log.api-enabled` startup option.

@RESTBODYPARAM{all,string,optional,string}
Pseudo-topic to address all log topics.

@RESTBODYPARAM{agency,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{agencycomm,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{agencystore,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{aql,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{arangosearch,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{audit-authentication,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{audit-authorization,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{audit-collection,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{audit-database,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{audit-document,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{audit-hotbackup,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{audit-service,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{audit-view,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{authentication,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{authorization,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{backup,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{bench,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{cache,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{cluster,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{clustercomm,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{collector,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{communication,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{config,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{crash,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{development,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{dump,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{engines,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{flush,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{general,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{graphs,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{heartbeat,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{httpclient,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{ldap,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{libiresearch,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{license,string,optional,string}
One of the possible log topics (_Enterprise Edition only_).

@RESTBODYPARAM{maintenance,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{memory,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{mmap,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{performance,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{pregel,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{queries,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{replication,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{requests,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{restore,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{rocksdb,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{security,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{ssl,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{startup,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{statistics,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{supervision,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{syscall,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{threads,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{trx,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{ttl,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{validation,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{v8,string,optional,string}
One of the possible log topics.

@RESTBODYPARAM{views,string,optional,string}
One of the possible log topics.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request is valid

@RESTRETURNCODE{400}
is returned when the request body contains invalid JSON.

@RESTRETURNCODE{403}
is returned if there are insufficient privileges to adjust log levels.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@endDocuBlock
