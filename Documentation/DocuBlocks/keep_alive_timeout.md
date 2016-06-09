
@startDocuBlock keep_alive_timeout
@brief timeout for HTTP keep-alive
`--http.keep-alive-timeout`

Allows to specify the timeout for HTTP keep-alive connections. The timeout
value must be specified in seconds.
Idle keep-alive connections will be closed by the server automatically
when the timeout is reached. A keep-alive-timeout value 0 will disable the keep
alive feature entirely.
@endDocuBlock

