Clusters Options
================

Agency endpoint
---------------

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

My address
----------

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

Listen only on interface with address `192.168.1.1`
```
--cluster.my-address tcp://192.168.1.1:8530
```

Listen on all ipv4 and ipv6 addresses, which are configured on port `8530`
```
--cluster.my-address ssl://[::]:8530
```

My role
-------

<!-- arangod/Cluster/ClusterFeature.h -->


This server's role:
`--cluster.my-role [dbserver|coordinator]`

The server's role. Is this instance a db server (backend data server)
or a coordinator (frontend server for external and application access)

Node ID (deprecated)
--------------------

<!-- arangod/Cluster/ClusterFeature.h -->


This server's id: `--cluster.my-local-info info`

Some local information about the server in the cluster, this can for
example be an IP address with a process ID or any string unique to
the server. Specifying *info* is mandatory on startup if the server
id (see below) is not specified. Each server of the cluster must
have a unique local info. This is ignored if my-id below is specified.

This option is deprecated and will be removed in a future release. The
cluster node ids have been dropped in favour of once generated UUIDs.

More advanced options
---------------------

{% hint 'warn' %}
These options should generally remain untouched.
{% endhint %}

<!-- arangod/Cluster/ClusterFeature.h -->


Synchroneous replication timing: `--cluster.synchronous-replication-timeout-factor double`

Strech or clinch timeouts for internal synchroneous replication
mechanism between db servers. All such timeouts are affected by this
change. Please change only with intent and great care. Default at `1.0`.

System replication factor: `--cluster.system-replication-factorinteger`

Change default replication factor for system collections. Default at `2`.
