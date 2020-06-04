

@brief this server's address / endpoint
`--cluster.my-address endpoint`

The server's endpoint for cluster-internal communication. If specified, it
must have the following pattern:
- tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
- tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
- ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
- ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption

If no *endpoint* is specified, the server will look up its internal
endpoint address in the Agency. If no endpoint can be found in the Agency
for the server's id, ArangoDB will refuse to start.

@EXAMPLES

```
--cluster.my-address tcp://192.168.1.1:8530
```

