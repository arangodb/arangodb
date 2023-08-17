@startDocuBlock get_admin_server_id

@RESTHEADER{GET /_admin/server/id, Get the server ID, getServerId}

@RESTDESCRIPTION
Returns the ID of a server in a cluster. The request will fail if the
server is not running in cluster mode.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned when the server is running in cluster mode.

@RESTRETURNCODE{500}
Is returned when the server is not running in cluster mode.

@endDocuBlock
