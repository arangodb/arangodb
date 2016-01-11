

@brief log file for requests
`--log.requests-file filename`

This option allows the user to specify the name of a file to which
requests are logged. By default, no log file is used and requests are
not logged. Note that if the file named by *filename* does not
exist, it will be created. If the file cannot be created (e.g. due to
missing file privileges), the server will refuse to start. If the
specified
file already exists, output is appended to that file.

Use *+* to log to standard error. Use *-* to log to standard output.
Use *""* to disable request logging altogether.

The log format is
- `"http-request"`: static string indicating that an HTTP request was
logged
- client address: IP address of client
- HTTP method type, e.g. `GET`, `POST`
- HTTP version, e.g. `HTTP/1.1`
- HTTP response code, e.g. 200
- request body length in bytes
- response body length in bytes
- server request processing time, containing the time span between
fetching
  the first byte of the HTTP request and the start of the HTTP response

