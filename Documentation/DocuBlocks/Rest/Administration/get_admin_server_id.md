
@startDocuBlock get_admin_server_id
@brief Get to know the internal id of the server

@RESTHEADER{GET /_admin/server/id, Return id of a server in a cluster, handleId}

@RESTDESCRIPTION
Returns the id of a server in a cluster. The request will fail if the
server is not running in cluster mode.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned when the server is running in cluster mode.

@RESTRETURNCODE{500}
Is returned when the server is not running in cluster mode.
@endDocuBlock
