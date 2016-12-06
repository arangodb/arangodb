Managing Endpoints
==================

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

**EXAMPLES**

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

TCP Endpoints
-------------

Given a hostname:

`--server.endpoint tcp://hostname:port`

Given an IPv4 address:

`--server.endpoint tcp://ipv4-address:port`

Given an IPv6 address:

`--server.endpoint tcp://[ipv6-address]:port`

On one specific ethernet interface each port can only be bound **exactly
once**. You can look up your available interfaces using the *ifconfig* command
on Linux / MacOSX - the Windows equivalent is *ipconfig* ([See Wikipedia for
more details](http://en.wikipedia.org/wiki/Ifconfig)).  The general names of the
interfaces differ on OS's and hardwares they run on.  However, typically every
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
