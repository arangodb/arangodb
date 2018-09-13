# ArangoDB Server Clusters Options

## Agency endpoint

<!-- arangod/Cluster/ClusterFeature.h -->

List of agency endpoints:
`--cluster.agency-endpoint endpoint`

An agency endpoint the server can connect to. The option can be specified
multiple times, so the server can use a cluster of agency servers.
Endpoints have the following pattern:

- tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
- tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
- ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
- ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption

At least one endpoint must be specified or ArangoDB will refuse to start.
It is recommended to specify at least two endpoints so ArangoDB has an
alternative endpoint if one of them becomes unavailable.

**Examples**

```
--cluster.agency-endpoint tcp://192.168.1.1:4001 --cluster.agency-endpoint tcp://192.168.1.2:4002 ...
```

## My address

<!-- arangod/Cluster/ClusterFeature.h -->

This server's address / endpoint:
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

**Examples**

Listen only on interface with address `192.168.1.1`:

```
--cluster.my-address tcp://192.168.1.1:8530
```

Listen on all ipv4 and ipv6 addresses, which are configured on port `8530`:

```
--cluster.my-address ssl://[::]:8530
```

## My advertised endpoint

<!-- arangod/Cluster/ClusterFeature.h -->

this server's advertised endpoint (e.g. external IP address or load balancer, optional)
`--cluster.my-advertised-endpoint`

This servers's endpoint for external communication. If specified, it
must have the following pattern:
- tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
- tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
- ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
- ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption

If no *advertised endpoint* is specified, no external endpoint will be advertised.

**Examples**

If an external interface is available to this server, it can be
specified to communicate with external software / drivers:

```
--cluster.my-advertised-enpoint tcp://some.public.place:8530
```

All specifications of endpoints apply.


## My role

<!-- arangod/Cluster/ClusterFeature.h -->

This server's role:
`--cluster.my-role [dbserver|coordinator]`

The server's role. Is this instance a db server (backend data server)
or a coordinator (frontend server for external and application access)

## Require existing ID

Require an existing server id: `--cluster.require-persisted-id bool`

If set to true, then the instance will only start if a UUID file is found 
in the database on startup. Setting this option will make sure the instance 
is started using an already existing database directory from a previous
start, and not a new one. For the first start, the UUID file must either be 
created manually in the database directory, or the option must be set to 
false for the initial startup and only turned on for restarts.

## More advanced options

{% hint 'warning' %}
These options should generally remain untouched.
{% endhint %}

<!-- arangod/Cluster/ClusterFeature.h -->

Synchronous replication timing:
`--cluster.synchronous-replication-timeout-factor double`

Stretch or clinch timeouts for internal synchronous replication
mechanism between db servers. All such timeouts are affected by this
change. Please change only with intent and great care. Default at `1.0`.

System replication factor: `--cluster.system-replication-factor integer`

Change default replication factor for system collections. Default at `2`.
