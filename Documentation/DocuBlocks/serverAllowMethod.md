

@brief allow HTTP method override via custom headers?
`--http.allow-method-override`

When this option is set to *true*, the HTTP request method will optionally
be fetched from one of the following HTTP request headers if present in
the request:

- *x-http-method*
- *x-http-method-override*
- *x-method-override*

If the option is set to *true* and any of these headers is set, the
request method will be overridden by the value of the header. For example,
this allows issuing an HTTP DELETE request which to the outside world will
look like an HTTP GET request. This allows bypassing proxies and tools
that will only let certain request types pass.

Setting this option to *true* may impose a security risk so it should only
be used in controlled environments.

The default value for this option is *false*.

