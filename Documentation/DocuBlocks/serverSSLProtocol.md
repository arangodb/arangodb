

@brief SSL protocol type to use
`--ssl.protocol value`

Use this option to specify the default encryption protocol to be used. The
following variants are available:

- 1: SSLv2 (unsupported)
- 2: SSLv2 or SSLv3 (negotiated)
- 3: SSLv3
- 4: TLSv1
- 5: TLSv1.2
- 6: TLSv1.3
- 9: generic TLS (negotiated)

The default *value* is 9 (generic TLS), which will allow the negotiation of
the TLS version between the client and the server, dynamically choosing the
highest mutually supported version of TLS.

Note that SSLv2 is unsupported as of ArangoDB 3.4, because of the inherent 
security vulnerabilities in this protocol. Selecting SSLv2 as protocol will
abort the startup.

**Note**: this option is only relevant if at least one SSL endpoint is used.
