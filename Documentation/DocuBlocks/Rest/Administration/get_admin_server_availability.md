
@startDocuBlock get_admin_server_availability
@brief Return whether or not a server is available

@RESTHEADER{GET /_admin/server/availability, Return whether or not a server is available}

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
@endDocuBlock

