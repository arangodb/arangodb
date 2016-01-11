

@brief default API compatibility
`--server.default-api-compatibility`

This option can be used to determine the API compatibility of the ArangoDB
server. It expects an ArangoDB version number as an integer, calculated as
follows:

*10000 \* major + 100 \* minor (example: *10400* for ArangoDB 1.4)*

The value of this option will have an influence on some API return values
when the HTTP client used does not send any compatibility information.

In most cases it will be sufficient to not set this option explicitly but
to
keep the default value. However, in case an "old" ArangoDB client is used
that does not send any compatibility information and that cannot handle
the
responses of the current version of ArangoDB, it might be reasonable to
set
the option to an old version number to improve compatibility with older
clients.

