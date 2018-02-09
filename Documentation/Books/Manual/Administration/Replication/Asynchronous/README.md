Asynchronous replication
========================

Asynchronous replication works by logging every data modification on a *master* and replaying these events on a number of *slaves*.

Transactions are honored in replication, i.e. transactional write operations will 
become visible on slaves atomically.

As all write operations will be logged to a master database's write-ahead log, the 
replication in ArangoDB currently cannot be used for write-scaling. The main purposes 
of the replication in current ArangoDB are to provide read-scalability and "hot backups" 
of specific databases.

It is possible to connect multiple slave databases to the same master database. Slave
databases should be used as read-only instances, and no user-initiated write operations 
should be carried out on them. Otherwise data conflicts may occur that cannot be solved 
automatically, and that will make the replication stop.

In an asynchronous replication scenario slaves will *pull* changes 
from the master database. Slaves need to know to which master database they should 
connect to, but a master database is not aware of the slaves that replicate from it. 
When the network connection between the master database and a slave goes down, write 
operations on the master can continue normally. When the network is up again, slaves 
can reconnect to the master database and transfer the remaining changes. This will 
happen automatically provided slaves are configured appropriately.


#### Replication lag

In this setup, write operations are applied first in the master database, and applied 
in the slave database(s) afterwards. 

For example, let's assume a write operation is executed in the master database 
at point in time t0. To make a slave database apply the same operation, it must first 
fetch the write operation's data from master database's write-ahead log, then parse it and 
apply it locally. This will happen at some point in time after t0, let's say t1. 

The difference between t1 and t0 is called the *replication lag*, and it is unavoidable 
in asynchronous replication. The amount of replication lag depends on many factors, a 
few of which are:

* the network capacity between the slaves and the master
* the load of the master and the slaves
* the frequency in which slaves poll the master for updates

Between t0 and t1, the state of data on the master is newer than the state of data
on the slave(s). At point in time t1, the state of data on the master and slave(s)
is consistent again (provided no new data modifications happened on the master in
between). Thus, the replication will lead to an *eventually consistent* state of data.

#### Replication configuration

The replication is turned off by default. In order to create a master-slave setup,
the so-called *replication applier* needs to be enabled on the slave databases.

Replication is configured on a per-database level. If multiple database are to be 
replicated, the replication must be set up individually per database.

The replication applier on the slave can be used to perform a one-time synchronization
with the master (and then stop), or to perform an ongoing replication of changes. To
resume replication on slave restart, the *autoStart* attribute of the replication 
applier must be set to *true*. 

#### Replication overhead

As the master servers are logging any write operation in the write-ahead-log anyway replication doesn't cause any extra overhead on the master. However it will of course cause some overhead for the master to serve incoming read requests of the slaves. Returning the requested data is however a trivial task for the master and should not result in a notable performance degration in production.