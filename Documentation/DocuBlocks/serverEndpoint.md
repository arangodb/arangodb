
@startDocuBlock serverEndpoint
@brief endpoints for client HTTP requests
`--server.endpoint endpoint`

Specifies an *endpoint* for HTTP requests by clients. Endpoints have
the following pattern:
- tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
- tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
- ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
- ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption
- unix:///path/to/socket - Unix domain socket endpoint

If a TCP/IP endpoint is specified without a port number, then the default
port (8529) will be used.
If multiple endpoints need to be used, the option can be repeated multiple
times.

@EXAMPLES

```
unix> ./arangod --server.endpoint tcp://127.0.0.1:8529
                --server.endpoint ssl://127.0.0.1:8530
                --ssl.keyfile server.pem /tmp/vocbase
2012-07-26T07:07:47Z [8161] INFO using SSL protocol version 'TLSv1'
2012-07-26T07:07:48Z [8161] INFO using endpoint 'ssl://127.0.0.1:8530' for
http ssl requests
2012-07-26T07:07:48Z [8161] INFO using endpoint 'tcp://127.0.0.1:8529' for
http tcp requests
2012-07-26T07:07:49Z [8161] INFO ArangoDB (version 1.1.alpha) is ready for
business
2012-07-26T07:07:49Z [8161] INFO Have Fun!
```

**Note**: If you are using SSL-encrypted endpoints, you must also supply
the path to a server certificate using the `--ssl.keyfile` option.

Endpoints can also be changed at runtime.
Please refer to [HTTP Interface for Endpoints](../../../HTTP/Endpoints/index.html)
for more details.
@endDocuBlock

