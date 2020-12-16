
@startDocuBlock get_admin_server_availability
@brief Return whether or not a server is available

@RESTHEADER{GET /_admin/server/availability, Return whether or not a server is available, handleAvailability}

@RESTDESCRIPTION
Return availability information about a server.

This is a public API so it does *not* require authentication. It is meant to be
used only in the context of server monitoring only.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200 in case the server is up and running and usable for
arbitrary operations, is not set to read-only mode and is currently not a follower
in case of an active failover setup.

@RESTRETURNCODE{503}
HTTP 503 will be returned in case the server is during startup or during shutdown,
is set to read-only mode or is currently a follower in an active failover setup.

In addition, since ArangoDB version 3.7.6, HTTP 503 will be returned in case the 
fill grade of the scheduler queue exceeds the configured high-water mark (adjustable 
via startup option `--server.unavailability-queue-fill-grade`), which by default is 
set to 100 % of the maximum queue length. I
@endDocuBlock
