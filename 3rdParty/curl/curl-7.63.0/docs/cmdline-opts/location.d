Long: location
Short: L
Help: Follow redirects
Protocols: HTTP
---
If the server reports that the requested page has moved to a different
location (indicated with a Location: header and a 3XX response code), this
option will make curl redo the request on the new place. If used together with
--include or --head, headers from all requested pages will be shown. When
authentication is used, curl only sends its credentials to the initial
host. If a redirect takes curl to a different host, it won't be able to
intercept the user+password. See also --location-trusted on how to change
this. You can limit the amount of redirects to follow by using the
--max-redirs option.

When curl follows a redirect and the request is not a plain GET (for example
POST or PUT), it will do the following request with a GET if the HTTP response
was 301, 302, or 303. If the response code was any other 3xx code, curl will
re-send the following request using the same unmodified method.

You can tell curl to not change the non-GET request method to GET after a 30x
response by using the dedicated options for that: --post301, --post302 and
--post303.
