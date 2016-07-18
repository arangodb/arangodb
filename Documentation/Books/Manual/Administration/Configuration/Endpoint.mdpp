!CHAPTER JavaScript Interface for managing Endpoints

The ArangoDB server can listen for incoming requests on multiple *endpoints*.

The endpoints are normally specified either in ArangoDB's configuration file or on
the command-line, using the ["--server.endpoint"](../Configuration/Arangod.md) option.
The default endpoint for ArangoDB is *tcp://127.0.0.1:8529* or *tcp://localhost:8529*.
ArangoDB can also do a so called *broadcast bind* using *tcp://0.0.0.0:8529*. This way
it will be reachable on all interfaces of the host. This may be useful
on development systems that frequently change their network setup like laptops.

Each endpoint can optionally be restricted to a specific list of databases
only, thus allowing the usage of different port numbers for different databases.

This may be useful in multi-tenant setups. 
A multi-endpoint setup may also be useful to turn on encrypted communication for
just specific databases.

Endpoints equal TCP ports to be bound. On one specific ethernet interface each port
can only be bound **exactly once**. You can look up your available interfaces using
the *ifconfig* command on Linux / MacOSX - the Windows equivalent is *ipconfig*
([See Wikipedia for more details](http://en.wikipedia.org/wiki/Ifconfig)).
The general names of the interfaces differ on OS's and hardwares they run on.
However, typically every host has a so called
[loopback interface](http://en.wikipedia.org/wiki/Loop_device), which is a
virtual interface. By convention it always has the address *127.0.0.1* or *::1* (ipv6),
and can only be reached from exactly the very same host.
Ethernet interfaces usually have names like *eth0*, *wlan0*, *eth1:17*, *le0* or
a plain text name in Windows.

To find out which services already use ports (so ArangoDB can't bind them anymore),
you can use the [netstat command](http://en.wikipedia.org/wiki/Netstat)
(it behaves a little different on each platform, run it with *-lnpt* on Linux, *-p tcp*
on MacOSX or with *-an* on windows for valuable information).

The JavaScript interface for endpoints provides an operation for retrieving the
list of endpoints. There is no functionality to add, remove or reconfigure an 
existing endpoint via the API after server start.

Please note that the operation to retrieve the list of endpoints can only be carried 
out in the default database (*_system*) and none of the other databases.
When not in the default database, you must first switch to it using the 
*db._useDatabase* method.

!SUBSECTION List

<!-- js/server/modules/@arangodb/arango-database.js -->

Returns a list of all endpoints and their mapped databases: `db._endpoints()`

Please note that managing endpoints can only be performed from out of the
*_system* database. When not in the default database, you must first switch
to it using the "db._useDatabase" method.
