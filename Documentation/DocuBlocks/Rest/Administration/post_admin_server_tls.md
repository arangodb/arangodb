
@startDocuBlock post_admin_server_tls
@brief Trigger a reload of the TLS data of this server (server key, client-auth CA) and return the new data as a summary.

@RESTHEADER{POST /_admin/server/tls, Trigger a reload of the TLS data and return a summary, handleTLS:post}

@RESTDESCRIPTION
This API call triggers a reload of all the TLS data and then
returns a summary. The JSON response is exactly as in the corresponding
GET request (see there).

This is a protected API and can only be executed with superuser rights.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200 if everything is ok

@RESTRETURNCODE{403}
This API will return HTTP 403 FORBIDDEN if it is not called with
superuser rights.
@endDocuBlock
