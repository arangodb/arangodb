!CHAPTER Introduction to Replication

Starting with ArangoDB 1.4, ArangoDB comes with optional asynchronous master-slave 
replication. Replication is configured on a per-database level, meaning that 
different databases in the same ArangoDB instance can have different replication
settings. Replication must be turned on explicitly before it becomes active for a
database.

In a typical master-slave replication setup, clients direct *all* their write 
operations for a specific database to the master. The master database is the only 
place to connect to when making any insertions/updates/deletions.

The master database will log all write operations in its so-called *event log*. 
The event log can be considered as an ordered stream of changes for the database.

Any number of slaves can then connect to the master database and fetch data from the
master database's event log. The slaves then can apply all the events from the log in 
the same order locally. After that, they will have the same state of data as the master
database.

In this setup, write operations are applied first in the master database, and applied 
in the slave database(s) afterwards. 

For example, let's assume a write operation is applied and logged in the master database 
at point in time t0. To make a slave database apply the same operation, it must first 
fetch the write operation's data from master database's event log, then parse it and 
apply it locally. This will happen at some point in time after t0, let's say t1. 

The difference between t1 and t0 is called the *replication lag*, and it is unavoidable 
in asynchronous replication. The amount of replication lag depends on many factors, a 
few of which are:
- the network capacity between the slaves and the master
- the load of the master and the slaves
- the frequency in which slaves poll the master for updates

Between t0 and t1, the state of data on the master is newer than the state of data
on the slave(s). At point in time t1, the state of data on the master and slave(s)
is consistent again (provided no new data modifications happened on the master in
between). Thus, the replication will lead to an *eventually consistent* state of data.

Transactions are honored in replication, i.e. transactional write operations will 
become visible on slaves atomically.

As all write operations will be logged to a master database's event log, the replication 
in ArangoDB 1.4 cannot be used for write-scaling. The main purposes of the replication
in ArangoDB 1.4 are to provide read-scalability and "hot backups" for specific databases.

It is possible to connect multiple slave databases to the same master database. Slave
databases should be used as read-only instances, and no user-initiated write operations 
should be carried out on them. Otherwise data conflicts may occur that cannot be solved 
automatically, and that will make the replication stop. Master-master (or multi-master) 
replication is not supported in ArangoDB 1.4.

The replication in ArangoDB 1.4 is asynchronous, meaning that slaves will *pull* 
changes from the master database. Slaves need to know to which master database they should 
connect to, but a master database is not aware of the slaves that replicate from it. 
When the network connection between the master database and a slave goes down, write 
operations on the master can continue normally. When the network is up again, slaves 
can reconnect to the master database and transfer the remaining changes. This will 
happen automatically provided slaves are configured appropriately.

The replication is turned off by default. In order to create a master-slave setup,
the replication features need to be enabled on both the master database and the slave
databases.

Replication is configured on a per-database level. If multiple database are to be 
replicated, the replication must be set up individually per database.

