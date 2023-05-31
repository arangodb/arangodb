@startDocuBlock get_admin_log_level
@brief returns the current log level settings

@RESTHEADER{GET /_admin/log/level, Return the current server log level, getLogLevel}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{serverId,string,optional}
Forwards the request to the specified server.

@RESTDESCRIPTION
Returns the server's current log level settings.
The result is a JSON object with the log topics being the object keys, and
the log levels being the object values.

This API can be turned off via the startup option `--log.api-enabled`. In case
the API is disabled, all requests will be responded to with HTTP 403. If the
API is enabled, accessing it requires admin privileges, or even superuser
privileges, depending on the value of the `--log.api-enabled` startup option.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request is valid

@RESTRETURNCODE{403}
is returned if there are insufficient privileges to read log levels.

@endDocuBlock
