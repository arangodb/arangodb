Clusters Options
================

### Node ID
<!-- arangod/Cluster/ApplicationCluster.h -->


This server's id: `--cluster.my-local-info info`

Some local information about the server in the cluster, this can for
example be an IP address with a process ID or any string unique to
the server. Specifying *info* is mandatory on startup if the server
id (see below) is not specified. Each server of the cluster must
have a unique local info. This is ignored if my-id below is specified.


### Agency endpoint
<!-- arangod/Cluster/ApplicationCluster.h -->


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
--cluster.agency-endpoint tcp://192.168.1.1:4001 --cluster.agency-endpoint
tcp://192.168.1.2:4002
```

### Agency prefix
<!-- arangod/Cluster/ApplicationCluster.h -->


Global agency prefix:
`--cluster.agency-prefix prefix`

The global key prefix used in all requests to the agency. The specified
prefix will become part of each agency key. Specifying the key prefix
allows managing multiple ArangoDB clusters with the same agency
server(s).

*prefix* must consist of the letters *a-z*, *A-Z* and the digits *0-9*
only. Specifying a prefix is mandatory.

**Examples**

```
--cluster.prefix mycluster
```

### MyId
<!-- arangod/Cluster/ApplicationCluster.h -->

This server's id: `--cluster.my-id id`

The local server's id in the cluster. Specifying *id* is mandatory on
startup. Each server of the cluster must have a unique id.

Specifying the id is very important because the server id is used for
determining the server's role and tasks in the cluster.

*id* must be a string consisting of the letters *a-z*, *A-Z* or the
digits *0-9* only.

### MyAddress

<!-- arangod/Cluster/ApplicationCluster.h -->


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

```
--cluster.my-address tcp://192.168.1.1:8530
```
