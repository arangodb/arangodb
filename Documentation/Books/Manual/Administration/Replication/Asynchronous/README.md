Asynchronous replication
========================

Asynchronous replication works by logging every data modification on a *Leader* server and replaying these events on a
onder or more *Follower* servers.

Transactions are honored in replication, i.e. transactional write, delete or update operations will
become visible on Followers atomically.

As all write operations will be logged to the Leader database's write-ahead log (WAL), the
replication in ArangoDB currently cannot be used for write-scaling. The main purposes 
of the replication in current ArangoDB are to provide read-scalability and "hot backups" 
of specific databases.

It is possible to connect multiple Follower server nodes to the same Leader node. Follower
databases should be used as read-only instances, and no user-initiated write operations 
should be carried out on them. Otherwise data conflicts may occur that cannot be solved 
automatically, and that will make the replication stop.

In an asynchronous replication scenario Followers will *pull* changes
from the server node. Followers need to know to which Leader database(s) they should
connect to, but a Leader is not aware of the Followers that replicate from it.
When the network connection between the Leader and a Follower goes down, write
operations on the Leader continue normally. When the network is up again, Followers
can reconnect to the Leader and transfer the remaining changes. This will
happen automatically provided Followers nodes are configured appropriately.


#### Replication Lag

In this setup, write operations are applied first in the Leader database, and applied
in the Follower(s) afterwards.

For example, let's assume a write operation is executed in the Leader
at point in time t0. To make a Follower apply the same operation, it must first
fetch the write operation's data from Leader's write-ahead log, then parse it and
apply it locally. This will happen at some point in time after *t0*, let's say *t1*.

The difference between t1 and t0 is called the *replication lag*, and it is unavoidable 
in asynchronous replication. The amount of replication lag depends on many factors, a 
few of which are:

* the network capacity between the two servers
* the load of the Lead server and the Followers
* the frequency in which Followers poll for updates

Between *t0* and *t1*, the state of data on the Leader is newer than the state of data
on the Follower(s). At point in time *t1*, the state of data is consistent again
(provided no new data modifications happened in the meanwhile).
Thus, the replication will lead to an **eventually consistent** state of data.

#### Replication configuration

The replication is turned off by default. In order to create a classical Leader-Follower (Also known as a classical
master-slave) setup, the so-called *replication applier* needs to be enabled on the Follower node.

Replication is configured on a per-database or server wide level. If multiple database but not all
databases are to be replicated, the replication must be set up individually per database.

The replication applier can be used to perform a one-time synchronization
with the Leader (and then stop), or to perform an ongoing replication of changes. To
resume replication on a restart, the *autoStart* attribute of the replication
applier must be set to *true*. 

#### Replication overhead

As the Leader servers are logging any write operation in the write-ahead-log anyway replication does not cause
any unreasonable overhead on the Leader node. However it will of course cause some overhead to serve
incoming read requests, namely it will use up some disk and network bandwidth. Returning the requested data is however
fairly a trivial task and should not result in a notable performance degration in production.
