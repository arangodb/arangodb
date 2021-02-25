

@brief list of Agency endpoints
`--cluster.agency-endpoint endpoint`

An Agency endpoint the server can connect to. The option can be specified
multiple times so the server can use a cluster of Agency servers.
Endpoints
have the following pattern:

- tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
- tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
- ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
- ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption

At least one endpoint must be specified or ArangoDB will refuse to start.
It is recommended to specify at least two endpoints so ArangoDB has an
alternative endpoint if one of them becomes unavailable.

@EXAMPLES

```
--cluster.agency-endpoint tcp://192.168.1.1:4001 --cluster.agency-endpoint
tcp://192.168.1.2:4002
```

