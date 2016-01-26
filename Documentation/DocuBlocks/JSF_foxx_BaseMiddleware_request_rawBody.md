


`request.rawBody()`

The raw request body, not parsed. The body is returned as a UTF-8 string.
Note that this can only be used sensibly if the request body contains
valid UTF-8. If the request body is known to contain non-UTF-8 data, the
request body can be accessed by using `request.rawBodyBuffer`.

