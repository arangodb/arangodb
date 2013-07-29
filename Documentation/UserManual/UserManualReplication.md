Replication {#UserManualReplication}
====================================

@NAVIGATE_UserManualReplication
@EMBEDTOC{UserManualReplicationTOC}

Introduction {#UserManualReplicationIntro}
==========================================

Starting with ArangoDB 1.4, ArangoDB comes with an optional master-slave replication.

The replication is asychronous and eventually consistent, meaning that slaves will 
*pull* changes from the master and apply them locally. Data on a slave may be
behind the state of data on the master until the slave has fetched and applied all 
changes. 

Transactions are honored in replication, i.e. changes by a replicated transaction will 
become visible on the slave atomically.

It is possible to connect multiple slaves to the same master. Slaves should be used as
read-only instances, though otherwise conflicts may occur that cannot be solved 
automatically in ArangoDB 1.4.
This is also the reason why master-master replication is not supported.

Components {#UserManualReplicationComponents}
=============================================

ArangoDB's replication consists of two main components, which can be used together or
separately: the *replication logger* and the *replication applier*.

Using both components on two ArangoDB servers provides master-slave replication between
the two, but there are also additional use cases.

Replication Logger {#UserManualReplicationLogger}
-------------------------------------------------

The purpose of the replication logger is to log all changes that modify data.
The replication logger will produce an ongoing stream of change events. That stream,
or specific parts of the stream can be queried by clients via an HTTP API.

An example client for this is the ArangoDB replication applier. 
The ArangoDB replication applier will permanently query the stream of change events 
the replication logger will write. It will apply "new" changes locally to get to 
the same state of data as the logger server.

External systems (e.g. indexers) could also incrementally query the log stream from 
the replication logger. Using this approach, one could feed external systems with all 
data modification operations done in ArangoDB.

The replication logger will write all change events to a system collection named
`_replication`. The events are thus persisted and still be present after a server
restart or crash.

ArangoDB will only log changes if the replication logger is turned on. Should there be 
any data modifications while the replication logger is turned off, these events will
be lost for replication.

The replication logger will mainly log events that affect user-defined collections. 
Operations on ArangoDB's system collections (collections with names that start with 
an underscore) are intentionally excluded from replication.

There is exactly one replication logger present in an ArangoDB database. 

Replication Applier {#UserManualReplicationApplier}
---------------------------------------------------

The purpose of the replication applier is to read data from a remote stream of change 
events from a data provider and apply them locally. The applier is thus using the
*pull* principle.

Normally, one would connect an ArangoDB replication applier to an ArangoDB replication
logger. This would make the applier fetch all data from the logger server incrementally.
The data on the applier thus will be a copy of the data on the logger server, and the
applier server can be used as a read-only or hot standby clone.

The applier can connect to any system that speaks HTTP and returns replication log 
events in the expected format (see @INTREF{HttpReplicationLoggerFollow,format} and @ref 
RefManualReplicationEventTypes). It is thus possible (though not the scope of the 
ArangoDB project) to implement data providers other than ArangoDB and still have an 
ArangoDB applier fetch their data and apply it.

As the replication applier does not get notified immediately when there are "new"
changes, it might take some time the applier has fetched and applied the newest changes 
from a logger server. Data modification operations might thus become visible on the
applying server later than on the server on which they were originated.

If the replication applier cannot connect to the data provider or the communication
fails for some reason, it will try to reconnect and fetch outstanding data. Until this
succeeds, the state of data on the replication applier might also be behind the state
of the data provider.

There is exactly one replication applier present in an ArangoDB database. It is thus
not possible to have an applier collect data from multiple ArangoDB "master" instances.

Setting up Replication {#UserManualReplicationSetup}
====================================================

Setting up a working replication topology requires two ArangoDB instances:
- the replication logger server (_master_): this is the instance we'll replication data from
- the replication applier server (_slave_): this instance will fetch data from the logger server 
  and apply all changes locally
  
For the following example setup, we'll use the instance *tcp://localhost:8529* as the 
logger server, and the instance *tcp://localhost:8530* as an applier.

The goal is to have all data from *tcp://localhost:8529* being replicated to the instance
*tcp://localhost:8530*.

Setting up the Logger {#UserManualReplicationSetupLogger}
---------------------------------------------------------



Setting up the Applier {#UserManualReplicationSetupApplier}
-----------------------------------------------------------


Replication Overhead {#UserManualReplicationOverhead}
=====================================================

Running the replication logger will make all data modification operations more 
expensive, as the ArangoDB server needs to write the operation into the replication log. 

Additionally, replication appliers that connect to an ArangoDB server will cause some
extra work as incoming HTTP requests need to be processed and results be generated.

Overall, turning on the replication logger will reduce throughput on an ArangoDB server
by some extent. If the replication feature is not required, the replication logger should 
be turned off.
