

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

