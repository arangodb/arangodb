@startDocuBlock get_admin_server_availability

@RESTHEADER{GET /_admin/server/availability, Return whether or not a server is available, getServerAvailability}

@RESTDESCRIPTION
Return availability information about a server.

The response is a JSON object with an attribute "mode". The "mode" can either
be "readonly", if the server is in read-only mode, or "default", if it is not.
Please note that the JSON object with "mode" is only returned in case the server
does not respond with HTTP response code 503.

This is a public API so it does *not* require authentication. It is meant to be
used only in the context of server monitoring.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200 in case the server is up and running and usable for
arbitrary operations, is not set to read-only mode and is currently not a follower
in case of an Active Failover deployment setup.

@RESTRETURNCODE{503}
HTTP 503 will be returned in case the server is during startup or during shutdown,
is set to read-only mode or is currently a follower in an Active Failover deployment setup.

In addition, HTTP 503 will be returned in case the fill grade of the scheduler
queue exceeds the configured high-water mark (adjustable via startup option
`--server.unavailability-queue-fill-grade`), which by default is set to 75 % of
the maximum queue length.

@endDocuBlock
