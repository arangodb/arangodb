

@brief this server's address / endpoint
`--cluster.my-address endpoint`

The server's endpoint for cluster-internal communication. If specified, it
must have the following pattern:
- tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
- tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
- ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
- ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption

If no *endpoint* is specified, the server will look up its internal
endpoint address in the agency. If no endpoint can be found in the agency
for the server's id, ArangoDB will refuse to start.

@EXAMPLES

```
--cluster.my-address tcp://192.168.1.1:8530
```





@brief this server's advertised endpoint (e.g. external IP address or load balancer, optional)
`--cluster.my-advertised-endpoint`

This servers's endpoint for external communication. If specified, it
must have the following pattern:
- tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
- tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
- ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
- ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption

If no *advertised endpoint* is specified, no external endpoint will be advertised.

@EXAMPLES

If an external interface is available to this server, it can be
specified to communicate with external software / drivers:

```
--cluster.my-advertised-enpoint tcp://some.public.place:8530
```

All specifications of endpoints apply.

