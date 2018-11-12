# ArangoDB Server _Server_ Options

## Managing Endpoints

The ArangoDB server can listen for incoming requests on multiple *endpoints*.

The endpoints are normally specified either in ArangoDB's configuration file or
on the command-line, using the `--server.endpoint`.  ArangoDB supports different
types of endpoints:

- tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
- tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
- ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
- ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption
- unix:///path/to/socket - Unix domain socket endpoint

If a TCP/IP endpoint is specified without a port number, then the default port
(8529) will be used.  If multiple endpoints need to be used, the option can be
repeated multiple times.

The default endpoint for ArangoDB is *tcp://127.0.0.1:8529* or
*tcp://localhost:8529*.

**Examples**

```
unix> ./arangod --server.endpoint tcp://127.0.0.1:8529
                --server.endpoint ssl://127.0.0.1:8530
                --ssl.keyfile server.pem /tmp/vocbase
2012-07-26T07:07:47Z [8161] INFO using SSL protocol version 'TLSv1'
2012-07-26T07:07:48Z [8161] INFO using endpoint 'ssl://127.0.0.1:8530' for http ssl requests
2012-07-26T07:07:48Z [8161] INFO using endpoint 'tcp://127.0.0.1:8529' for http tcp requests
2012-07-26T07:07:49Z [8161] INFO ArangoDB (version 1.1.alpha) is ready for business
2012-07-26T07:07:49Z [8161] INFO Have Fun!
```

Given a hostname:

`--server.endpoint tcp://hostname:port`

Given an IPv4 address:

`--server.endpoint tcp://ipv4-address:port`

Given an IPv6 address:

`--server.endpoint tcp://[ipv6-address]:port`

On one specific ethernet interface each port can only be bound **exactly
once**. You can look up your available interfaces using the *ifconfig* command
on Linux / MacOSX - the Windows equivalent is *ipconfig* ([See Wikipedia for
more details](http://en.wikipedia.org/wiki/Ifconfig)). The general names of the
interfaces differ on OS's and hardwares they run on. However, typically every
host has a so called
[loopback interface](http://en.wikipedia.org/wiki/Loop_device),
which is a virtual interface. By convention it always has the address
*127.0.0.1* or *::1* (ipv6), and can only be reached from exactly the very same
host.  Ethernet interfaces usually have names like *eth0*, *wlan0*, *eth1:17*,
*le0* or a plain text name in Windows.

To find out which services already use ports (so ArangoDB can't bind them
anymore), you can use the
[netstat command](http://en.wikipedia.org/wiki/Netstat)
(it behaves a little different on each platform, run it with *-lnpt* on Linux,
*-p tcp* on MacOSX or with *-an* on windows for valuable information).

ArangoDB can also do a so called *broadcast bind* using
*tcp://0.0.0.0:8529*. This way it will be reachable on all interfaces of the
host. This may be useful on development systems that frequently change their
network setup like laptops.

### Special note on IPv6 link-local addresses

ArangoDB can also listen to IPv6 link-local addresses via adding the zone ID
to the IPv6 address in the form `[ipv6-link-local-address%zone-id]`. However,
what you probably instead want is to bind to a local IPv6 address. Local IPv6
addresses start with `fd`. If you only see a `fe80:` IPv6 address in your
interface configuration but no IPv6 address starting with `fd` your interface
has no local IPv6 address assigned. You can read more about IPv6 link-local
addresses [here](https://en.wikipedia.org/wiki/Link-local_address#IPv6).

**Example**

Bind to a link-local and local IPv6 address.

    unix> ifconfig

This command lists all interfaces and assigned ip addresses. The link-local
address may be `fe80::6257:18ff:fe82:3ec6%eth0` (IPv6 address plus interface name).
A local IPv6 address may be `fd12:3456::789a`. To bind ArangoDB to it start
*arangod* with `--server.endpoint tcp://[fe80::6257:18ff:fe82:3ec6%eth0]:8529`.
Use telnet to test the connection.

    unix> telnet fe80::6257:18ff:fe82:3ec6%eth0 8529
    Trying fe80::6257:18ff:fe82:3ec6...
    Connected to my-machine.
    Escape character is '^]'.
    GET / HTTP/1.1
    
    HTTP/1.1 301 Moved Permanently
    Location: /_db/_system/_admin/aardvark/index.html
    Content-Type: text/html
    Server: ArangoDB
    Connection: Keep-Alive
    Content-Length: 197
    
    <html><head><title>Moved</title></head><body><h1>Moved</h1><p>This page has moved to <a href="/_db/_system/_admin/aardvark/index.html">/_db/_system/_admin/aardvark/index.html</a>.</p></body></html>

### Reuse address

`--tcp.reuse-address`

If this boolean option is set to *true* then the socket option SO_REUSEADDR is
set on all server endpoints, which is the default.  If this option is set to
*false* it is possible that it takes up to a minute after a server has
terminated until it is possible for a new server to use the same endpoint
again. This is why this is activated by default.

Please note however that under some operating systems this can be a security
risk because it might be possible for another process to bind to the same
address and port, possibly hijacking network traffic. Under Windows, ArangoDB
additionally sets the flag SO_EXCLUSIVEADDRUSE as a measure to alleviate this
problem.

### Backlog size

`--tcp.backlog-size`

Allows to specify the size of the backlog for the *listen* system call The
default value is 10. The maximum value is platform-dependent.  Specifying a
higher value than defined in the system header's SOMAXCONN may result in a
warning on server start. The actual value used by *listen* may also be silently
truncated on some platforms (this happens inside the *listen* system call).

## Maximal queue size

Maximum size of the queue for requests: `--server.maximal-queue-size
size`

Specifies the maximum *size* of the queue for asynchronous task
execution. If the queue already contains *size* tasks, new tasks will
be rejected until other tasks are popped from the queue. Setting this
value may help preventing from running out of memory if the queue is
filled up faster than the server can process requests.

## Storage engine

ArangoDB's "traditional" storage engine is called `MMFiles`, which also was the
default storage engine up to including ArangoDB 3.3.

Since ArangoDB 3.2, an alternative engine based on [RocksDB](http://rocksdb.org)
is also provided and could be turned on manually. Since ArangoDB 3.4, the RocksDB
storage engine is the default storage engine for new installations.

One storage engine type is supported per server per installation.
Live switching of storage engines on already installed systems isn't supported.
Configuring the wrong engine (not matching the previously used one) will result
in the server refusing to start. You may however use `auto` to let ArangoDB choose
the previously used one.

`--server.storage-engine [auto|mmfiles|rocksdb]`

Note that `auto` will default to `rocksdb` starting with ArangoDB 3.4, but in
previous versions it defaulted to `mmfiles`.

## Check max memory mappings

`--server.check-max-memory-mappings` can be used on Linux to make arangod
check the number of memory mappings currently used by the process (as reported in
`/proc/<pid>/maps`) and compare it with the maximum number of allowed mappings as
determined by */proc/sys/vm/max_map_count*. If the current number of memory
mappings gets near the maximum allowed value, arangod will log a warning
and disallow the creation of further V8 contexts temporarily until the current
number of mappings goes down again.

If the option is set to false, no such checks will be performed. All non-Linux
operating systems do not provide this option and will ignore it.

## Enable/disable authentication

@startDocuBlock server_authentication

## JWT Secret

`--server.jwt-secret secret`

ArangoDB will use JWTs to authenticate requests. Using this option let's
you specify a JWT. When specified, the JWT secret must be at most 64 bytes
long.

In single server setups and when not specifying this secret ArangoDB will
generate a secret.

In cluster deployments which have authentication enabled a secret must
be set consistently across all cluster nodes so they can talk to each other.

## Enable/disable authentication for UNIX domain sockets

`--server.authentication-unix-sockets value`

Setting *value* to true will turn off authentication on the server side
for requests coming in via UNIX domain sockets. With this flag enabled,
clients located on the same host as the ArangoDB server can use UNIX domain
sockets to connect to the server without authentication.
Requests coming in by other means (e.g. TCP/IP) are not affected by this option.

The default value is *false*.

**Note**: this option is only available on platforms that support UNIX
domain sockets.

## Enable/disable authentication for system API requests only

@startDocuBlock serverAuthenticateSystemOnly

## Enable authentication cache timeout

`--server.authentication-timeout value`

Sets the cache timeout to *value* (in seconds). This is only necessary
if you use an external authentication system like LDAP.

## Enable local authentication

`--server.local-authentication value`

If set to *false* only use the external authentication system. If
*true* also use the local *_users* collections.

The default value is *true*.

## Server threads

`--server.minimal-threads number`

`--server.maximal-threads number`

Specifies the *number* of threads that are spawned to handle requests.

The actual number of request processing threads is adjusted dynamically at runtime
and will float between `--server.minimal-threads` and `--server.maximal-threads`.

`--server.minimal-threads` determines the minimum number of request processing
threads the server will start and that will always be kept around. The default
value is *2*.

`--server.maximal-threads` determines the maximum number of request processing
threads the server is allowed to start for request handling. If that number of
threads is already running, arangod will not start further threads for request
handling. The default value is

## Toggling server statistics

`--server.statistics value`

If this option is *value* is *false*, then ArangoDB's statistics gathering
is turned off. Statistics gathering causes regular background CPU activity and
memory usage, so using this option to turn statistics off might relieve heavily-loaded 
instances a bit.

## Data source flush synchronization

`--server.flush-interval`

ArangoDB will periodically ensure that all data sources (databases, views, etc.)
have flushed all committed data to disk and write some checkpoint data to aid in
future recovery. Increasing this value will result in fewer, larger write
batches, while decreasing it will result in more, smaller writes. Setting the
value too low can easily overwhelm the server, while setting the value too high
may result in high memory usage and periodic slowdowns. Value is given in
microseconds, with a typical range of 100000 (100ms) to 10000000 (10s) and a
default of 1000000 (1s). Use caution when changing from the default.
