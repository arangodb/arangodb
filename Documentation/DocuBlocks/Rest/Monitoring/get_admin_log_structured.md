@startDocuBlock get_admin_log_structured

@RESTHEADER{GET /_admin/log/structured, Get the structured log settings, getStructuredLog}

@RESTDESCRIPTION
Returns the server's current structured log settings.
The result is a JSON object with the log parameters being the object keys, and
`true` or `false` being the object values, meaning the parameters are either 
enabled or disabled.

This API can be turned off via the startup option `--log.api-enabled`. In case
the API is disabled, all requests will be responded to with HTTP 403. If the
API is enabled, accessing it requires admin privileges, or even superuser
privileges, depending on the value of the `--log.api-enabled` startup option.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the request is valid

@RESTRETURNCODE{403}
is returned if there are insufficient privileges to read structured log 
parameters.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.

@endDocuBlock
