# Active Failover Architecture

Consider the case for two *arangod* instances:
Two servers are connected via server wide asynchronous replication. One of the servers is
elected leader, and the other one is made a follower automatically. At startup,
the two servers fight for leadership. The follower will automatically start
replication from the master for all available databases, using the server-level
replication introduced in 3.3.

When the master goes down, this is automatically detected by an agency
instance, which is also started in this mode. This instance will make the
previous follower stop its replication and make it the new leader.

The follower will automatically deny all read and write requests from client
applications. Only the replication itself is allowed to access the follower's data
until the follower becomes a new leader.

When sending a request to read or write data on a follower, the follower will 
always respond with `HTTP 503 (Service unavailable)` and provide the address of
the current leader. Client applications and drivers can use this information to 
then make a follow-up request to the proper leader:

```
HTTP/1.1 503 Service Unavailable
X-Arango-Endpoint: http://[::1]:8531
....
```

Client applications can also detect who the current leader and the followers
are by calling the `/_api/cluster/endpoints` REST API. This API is accessible
on leaders and followers alike.

The ArangoDB starter supports starting two servers with asynchronous
replication and failover out of the box.

The arangojs driver for JavaScript, the Go driver and the Java driver for
ArangoDB support automatic failover in case the currently accessed server endpoint 
responds with HTTP 503.

Blog article:
[Introducing the new ArangoDB Java driver with load balancing and advanced fallback
](https://www.arangodb.com/2017/12/introducing-the-new-arangodb-java-driver-load-balancing/