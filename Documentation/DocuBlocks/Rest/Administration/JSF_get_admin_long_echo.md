
@startDocuBlock JSF_get_admin_long_echo
@brief Send back what was sent in, headers, post body etc.

@RESTHEADER{GET /_admin/long_echo, Return current request and continues}

@RESTDESCRIPTION

The call returns an object with the following attributes:

- *headers*: object with HTTP headers received

- *requestType*: the HTTP request method (e.g. GET)

- *parameters*: object with query parameters received

@RESTRETURNCODES

@RESTRETURNCODE{200}
Echo was returned successfully.
@endDocuBlock

