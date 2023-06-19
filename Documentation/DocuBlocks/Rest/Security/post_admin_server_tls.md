
@startDocuBlock post_admin_server_tls

@RESTHEADER{POST /_admin/server/tls, Reload the TLS data, reloadServerTls}

@RESTDESCRIPTION
This API call triggers a reload of all the TLS data (server key, client-auth CA)
and then returns a summary. The JSON response is exactly as in the corresponding
GET request.

This is a protected API and can only be executed with superuser rights.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200 if everything is ok

@RESTRETURNCODE{403}
This API will return HTTP 403 Forbidden if it is not called with
superuser rights.
@endDocuBlock
