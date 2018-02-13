# Active Failover Architecture

Consider the case for two *arangod* instances:

![Simple Leader / Follower setup, with a single node agency](leader-follower.png)

Two servers are connected via server wide asynchronous replication. One of the servers is
elected _Leader_, and the other one is made a _Follower_ automatically. At startup,
the two servers race for the leadership position. This happens through the agency
locking mechanisms (which means the Agency needs to be available at server start).
You can control which server will become Leader by starting it earlier than
other server instances in the beginning.


The _Follower_ will automatically start
replication from the master for all available databases, using the server-level
replication introduced in 3.3.
When the master goes down, this is automatically detected by an agency
instance, which is also started in this mode. This instance will make the
previous follower stop its replication and make it the new leader.

The follower will deny all read and write requests from client
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

The arangojs driver for JavaScript, the Go driver, the Java driver, ArangoJS and the PHP driver
support active failover in case the currently accessed server endpoint 
responds with `HTTP 503`.
