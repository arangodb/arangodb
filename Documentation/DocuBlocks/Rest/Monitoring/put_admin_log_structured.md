@startDocuBlock put_admin_log_structured
@brief modifies the current structured log settings

@RESTHEADER{PUT /_admin/log/structured, Modify and return the current server log parameters, setStructuredLog}

@RESTDESCRIPTION
Modifies and returns the server's current structured log settings.
The request body must be a JSON object with the structured log parameters 
being the object keys and `true` or `false` object values, for either 
enabling or disabling the parameters.

The result is a JSON object with all available structured log parameters being
the object keys, and `true` or `false` being the object values, meaning the 
parameter in the object key is either enabled or disabled.

This API can be turned off via the startup option `--log.api-enabled`. In case
the API is disabled, all requests will be responded to with HTTP 403. If the
API is enabled, accessing it requires admin privileges, or even superuser
privileges, depending on the value of the `--log.api-enabled` startup option.

@RESTBODYPARAM{database,boolean,optional,}
One of the possible log parameters.

@RESTBODYPARAM{username,boolean,optional,}
One of the possible log parameters.

@RESTBODYPARAM{url,boolean,optional,}
One of the possible log parameters.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request is valid

@RESTRETURNCODE{403}
is returned if there are insufficient privileges to adjust log levels.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@endDocuBlock
