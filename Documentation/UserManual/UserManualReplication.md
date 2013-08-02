Replication {#UserManualReplication}
====================================

@NAVIGATE_UserManualReplication
@EMBEDTOC{UserManualReplicationTOC}

Introduction to Replication {#UserManualReplicationIntro}
=========================================================

Starting with ArangoDB 1.4, ArangoDB comes with optional asynchronous master-slave 
replication.

In a typical master-slave replication setup, clients direct *all* their write 
operations to the master. The master is the only place to connect to when making
any insertions/updates/deletions.

The master will log all write operations in its so-called *event log*. The event log
can be considered as an ordered stream of changes.

Any number of slave servers can then connect to the master and fetch data from the
master's event log. The slaves then can apply all the events from the log in the 
same order locally. After that, they will have the same state of data as the master.

In this setup, write operations are applied first on the master, and applied on the
slave afterwards. For example, let's assume a write operation is applied and logged 
on the master at point in time t0. 

To make a slave apply the same operation, it must first fetch the write operation's 
data from master's event log, then parse it and apply it locally. This will happen
after t0, let's say at point in time t1. The difference between t1 and t0 is called the
*replication lag*, and it is unavoidable in asychronous replication. The amount of
replication lag depends on many factors. A few of them are:
- the network capacity between the slaves and the master
- the load and the master and the slaves
- the frequency in which slaves poll the master for updates

Between t0 and t1, the state of data on the master is newer than the state of data
on the slave(s). At point in time t1, the state of data on the master and slave(s)
is consistent again (provided no new data modifications happened on the master in
between). Thus, the replication will lead to an *eventually consistent* state of data.

Transactions are honored in replication, i.e. transactional write operations will 
become visible on slaves atomically.

As all write operations will be logged to the master's event log, the replication in
ArangoDB 1.4 cannot be used for write-scaling. The main purposes of the replication
in ArangoDB 1.4 are to provide read-scalability and "hot backups".

It is possible to connect multiple slaves to the same master. Slaves should be used as
read-only instances, and no user-initiated write operations should be carried out on 
them. Otherwise data conflicts may occur that cannot be solved automatically, and that
will make the replication stop. Master-master (or multi-master) replication is not
supported in ArangoDB 1.4.

The replication in ArangoDB 1.4 is asychronous, meaning that slaves will *pull* 
changes from the master. Slaves need to know to which master they should connect to,
but a master is not aware of the slaves that replicate from it. When the network
connection between the master and a slave goes down, write operations on the 
master can continue normally. When the network is up again, slaves can reconnect 
to the master and transfer the remaining changes. This will happen automatically 
provided slaves are configured appropriately.

The replication is turned off by default. In order to create a master-slave setup,
the replication features need to be enabled on both the master and the slave(s).

Components {#UserManualReplicationComponents}
=============================================

The replication architecture in ArangoDB 1.4 consists of two main components, which 
can be used together or in isolation: the *replication logger* and the *replication applier*.

Both are available in ArangoDB 1.4 and can be administered via the command line or 
a REST API (see @ref HttpReplication).

In most cases, the *replication logger* will be run on a master, and the 
*replication applier* will be executed on slave servers.

Replication Logger {#UserManualReplicationLogger}
-------------------------------------------------

###Purpose

The purpose of the replication logger is to log all changes that modify the state
of data. This includes document insertions, updates, and deletions. It also includes
creating, dropping, renaming and changing collections and indexes.

When the replication logger is used, it will log all these write operations in its
*event log*, in the same order in which the operations were carried out originally.

Reading the event log sequentially provides a list of all write operations carried out.
Replication clients can request certain log events from the logger.

For example, ArangoDB's replication applier will permanently query the latest events 
from the replication logger. The applier will then apply all changes locally to get to 
the same state of data as the master server. It will keep track of which parts of
the event log it already synchronised, meaning that it will perform incremental
synchronisation.

Technically, the event log is a system collection named `_replication`. The event log
is persisted and will still be present after a server shutdown or crash. The event log's
underlying collection should not be modified by users directly. It should ony be 
accessed using the special API methods offered by ArangoDB.

###Starting and Stopping

ArangoDB will only log changes if the replication logger is turned on. Should there be 
any write operations while the replication logger is turned off, these events will
be lost for replication.

To turn on the replication logger once, the following command can be executed:

    require("org/arangodb/replication").logger.start();

Note that starting the replication logger does not necessarily mean it will be started
automatically on all following server startups. This can be configured seperately (keep on
reading for this).

To turn the replication logger off, execute the `stop` command:
    
    require("org/arangodb/replication").logger.stop();

This has turned off the logger, but still the logger may be started again automatically
the next time the ArangoDB server is started. This can be configured separately (again,
keep on reading).

To determine the current state of the replication logger (including whether it is currently
running or not), use the `state` command:
    
    require("org/arangodb/replication").logger.state();

The result might look like this:

    { 
      "state" : { 
        "running" : false, 
        "lastLogTick" : "255376126918573", 
        "totalEvents" : 0, 
        "time" : "2013-08-02T11:01:28Z" 
      }, 
      "server" : { 
        "version" : "1.4.devel", 
        "serverId" : "53904504772335" 
      }, 
      "clients" : [ ] 
    }

The `running` attribute indicates whether the logger is currently enabled and will
log any events. The `totalEvents` attribute will indicate how many log events have been
logged since the start of the ArangoDB server. The value will not be reset between
multiple stops and restarts of the logger. Finally, the `lastLogTick` value will indicate
the id of the last event that was logged. It can be used to determine whether new 
events were logged, and is also used by the replication applier for incremental 
fetching of data.

Note: the replication logger state can also be queried via the HTTP API (see @ref HttpReplication).

###Configuration

To determine whether the replication logger is automatically started when the ArangoDB
server is started, the logger has a separate configuration. The configuration is stored
in a file `REPLICATION-LOGGER-CONFIG` inside the database directory. If it does not 
exist, ArangoDB will use default values.

To check and adjust the configuration of the replication logger, use the `properties` command.
To view the current configuration, use `properties` without any arguments:
    
    require("org/arangodb/replication").logger.properties();

The result might look like this:

    { 
      "autoStart" : false, 
      "logRemoteChanges" : false, 
      "maxEvents" : 1048576, 
      "maxEventsSize" : 134217728 
    }

The `autoStart` attribute indicates whether the replication logger is automatically started
whenever the ArangoDB server is started. You may want to set it to `true` for a proper master
setup. To do so, supply the updated attributes to the `properties` command, e.g.:
    
    require("org/arangodb/replication").logger.properties({ 
      autoStart: true 
    });

This will ensure the replication logger is automatically started on all following ArangoDB 
starts unless `autoStart` is set to `false` again.

Note that only those attributes will be changed that you supplied in the argument to `properties`.
All other configuration values will remain unchanged. Also note that the replication logger
can be reconfigured while it is running.

It the replication logger is turned on, the event log may be allowed to grow indefinitely.
It may be sensible to set a maximum size or a maximum number of events to keep. 
If one of these thresholds is reached during logging, the replication logger will start
removing the oldest events from the event log automatically. 

The thresholds are set by adjusting the `maxEvents` and `maxEventsSize` attributes via the
`properties` command. The `maxEvents` attribute is the maximum number of events to keep 
in the event log before starting to remove oldest events. The `maxEventsSize` is the maximum 
cumulated size of event log data (in bytes) that is kept in the log before starting to remove
oldest events. If both are set to a value of `0`, it means that the number and size
of log events to keep is unrestricted. If both are set to a non-zero value, it means
that there are restrictions on both the number and cumulated size of events to keep, 
and if either of the restrictions is hit, the deletion will be triggered.

The following command will set the threshold to 5000 events to keep, irrespective of the
cumulated event data sizes:
    
    require("org/arangodb/replication").logger.properties({ 
      maxEvents: 5000, 
      maxEventsSize: 0 
    });


Replication Applier {#UserManualReplicationApplier}
---------------------------------------------------

###Purpose

The purpose of the replication applier is to read data from a master's event log, and
apply them locally. The applier will check the master for new events periodically. 
It will perform an incremental synchronisation, i.e. only asking the master for events 
that occurred after the last synchronisation.

The replication applier does not get notified by the master when there are "new" events, 
but uses the pull principle. It might thus take some time (the *replication lag*) before 
an event that from the master gets shipped to and applied on a slave. 

The replication applier is run in a separate thread. It may encounter problems when a
log event from the master cannot be applied safely, or when the connection to the master
goes down (network outage, master is down etc.). In this case, the replication applier
thread might terminate itself. It is then up to the administrator to fix the problem and
restart the replication applier.

If the replication applier cannot connect to the master, or the communication fails at
some point during the synchronisation, the replication applier will try to reconnect to
the master. It will give up reconnecting only after a configurable amount of connection 
attempts.

The replication applier state is queryable at any time by using the `state` command of the
applier:

    require("org/arangodb/replication").applier.state();

The result might look like this:

    { 
      "state" : { 
        "running" : false, 
        "lastAppliedContinuousTick" : "231848832948633", 
        "lastProcessedContinuousTick" : "231848832948633", 
        "lastAvailableContinuousTick" : null, 
        "progress" : { 
          "time" : "2013-08-02T11:40:08Z", 
          "message" : "applier created", 
          "failedConnects" : 0 
        }, 
        "totalRequests" : 0, 
        "totalFailedConnects" : 0, 
        "totalEvents" : 0, 
        "lastError" : { 
          "errorNum" : 0 
        }, 
        "time" : "2013-08-02T11:40:22Z" 
      }, 
      "server" : { 
        "version" : "1.4.devel", 
        "serverId" : "53904504772335" 
      }, 
      "endpoint" : "tcp://master.domain.org:8529" 
    }

The `running` attribute indicates whether the replication applier is currently running
and polling the server at `endpoint` for new events.

The `failedConnects` attribute shows how many failed connection attempts the replication
applier currently has encountered in a row. In contrast, the `totalFailedConnects` attribute
indicates how many failed connection attempts the applier has made in total. The
`totalRequest` attribute shows how many requests the applier has sent to the master in
total. The `totalEvents` attribute shows how many log events the applier has read from the
master. 

The `message` sub-attribute of the `progress` sub-attribute gives a brief hint of what the
applier currently does (if it is running). The `lastError` attribute also has an optional
`errorMessage` sub-attribute, showing the latest error message. The `errorNum` sub-attribute of the
`lastError` attribute can be used by clients to programmatically check for errors. It should be `0`
if there is no error, and it should be non-zero when the applier terminated itself due to
a problem.

Here is an example of the state after the replication applier terminated itself due to 
(repeated) connection problems:

    { 
      "state" : { 
        "running" : false, 
        "progress" : { 
          "time" : "2013-08-02T12:40:25Z", 
          "message" : "applier stopped", 
          "failedConnects" : 6 
        }, 
        "totalRequests" : 18, 
        "totalFailedConnects" : 11, 
        "totalEvents" : 0, 
        "lastError" : { 
          "time" : "2013-08-02T12:40:25Z", 
          "errorMessage" : "could not connect to master at tcp://master.example.org:8529: Could not connect to 'tcp:/...", 
          "errorNum" : 1400 
        },
        ...
      }
    }

Note: the state of the replication applier is queryable via the HTTP API, too. Please refer to
@ref HttpReplication for more details.

###Starting and Stopping

To start and stop the applier, the `start` and `stop` commands can be used:
    
    require("org/arangodb/replication").applier.start(<tick>);
    require("org/arangodb/replication").applier.stop();

Note that starting a replication applier without setting up an initial configuration will 
fail. The replication applier will look for its configuration in a file named 
`REPLICATION-APPLIER-CONFIG` in the database directory. If the file is not present, ArangoDB
will use some default configuration, but it cannot guess the endpoint (the address of the master)
the applier should connect to. Thus starting the applier without configuration will fail.

Note that starting the replication applier via the `start` command will not necessarily start
the applier on the next and following ArangoDB server restarts. Additionally, stopping the applier 
manually will not necessarily prevent the applier from being started again on the next
server start. All of this is configurable seperately (hang on reading).

Please note that when starting the replication applier, it will resume where it stopped. 
This is sensible because replication log events should be applied incrementally. If the
replication applier has never been started before, it needs some `tick` value from the
master's event log from which to start fetching events.

####Configuration

To configure the replication applier, use the `properties` command. Using it without any
arguments will return the current configuration:
    
    require("org/arangodb/replication").applier.properties();

The result might look like this:

    { 
      "requestTimeout" : 300, 
      "connectTimeout" : 10, 
      "ignoreErrors" : 0, 
      "maxConnectRetries" : 10, 
      "chunkSize" : 0, 
      "autoStart" : false, 
      "adaptivePolling" : true 
    }

Note that there is no `endpoint` attribute configured yet. The `endpoint` attribute is required
for the replication applier to be startable. You may also want to configure a username and password
for the connection via the `username` and `password` attributes. 

    require("org/arangodb/replication").applier.properties({ 
      endpoint: "tcp://master.domain.org:8529", 
      username:  "root", 
      password: "secret"
    });

This will re-configure the replication applier. The configuration will be used from the next
start of the replication applier. The replication applier cannot be re-configured while it is
running. It must be stopped first to be re-configured.

To make the replication applier start automatically when the ArangoDB server starts, use the
`autoStart` attribute. 

Setting the `adaptivePolling` attribute to `true` will make the replication applier poll the 
master for changes with a variable frequency. The replication applier will then lower the 
frequency when the master is idle, and increase it when the master can provide new events).
Otherwise the replication applier will poll the master for changes with a constant frequency of
5 seconds if the master's replication logger is turned off, and 0.5 seconds if it is turned on.

To set a timeout for connection and following request attempts, use the `connectTimeout` and 
`requestTimeout` values. The `maxConnectRetries` attribute configures after how many failed 
connection attempts in a row the replication applier will give up and turn itself off. 
You may want to set this to a high value so that temporary network outages do not lead to the 
replication applier stopping itself.

The `chunkSize` attribute can be used to control the approximate maximum size of a master's
reponse (in bytes). Setting it to a low value may make the master respond faster (less data is
assembled before the master sends the response), but may require more request-response roundtrips.
Set it to `0` to use ArangoDB's built-in default value.

The following example will set most of the discussed properties:
    
    require("org/arangodb/replication").applier.properties({ 
      endpoint: "tcp://master.domain.org:8529", 
      username:  "root", 
      password: "secret",
      adaptivePolling: true,
      connectTimeout: 15,
      maxConnectRetries: 100,
      chunkSize: 262144,
      autoStart: true 
    });

After the applier is now fully configured, it could theoretically be started. However, we
may first need an initial sychronisation of all collections and their data from the master before
we start the replication applier. The reason is that the replication logger on the master
may have been turned on the first after some collections have been created, or it may have
been turned off temporarily etc.

The only safe method for initially starting the continuous replication applier is thus to
do a full synchronisation with the master first, note the master's current `tick` value, and
start the continuous replication applier using this tick value.

The initial synchronisation is executed with the `sync` command:
    
    require("org/arangodb/replication").sync({
      endpoint: "tcp://master.domain.org:8529",
      username: "root",
      password: "secret
    });

Warning: `sync` will do a full synchronisation of the collections present on the master.
Any local instances of the collections and all their data are removed! Only execute this
command when you are sure you want to remove the local data!

As `sync` does a full synchronisation, it may take a while to execute.
When `sync` completes successfully, it show a list of collections it has synchronised in the
`collections` attribute. It will also return the master's replication logger tick value at
the time the `sync` was started on the master. The tick value is contained in the `lastLogTick`
attribute of the `sync` command: 

    { 
      "lastLogTick" : "231848833079705", 
      "collections" : [ ... ]
    }

If the master's replication logger is turned on, you can now start the continuous synchronisation
with the command

    require("org/arangodb/replication").applier.start("231848833079705");

Note that the tick values should be handled as strings. Using numeric data types for tick
values is unsafe because they might exceed the 32 bit value and the IEEE754 double accuracy
ranges.

Example Setup {#UserManualReplicationSetup}
===========================================

Setting up a working master-slave replication requires two ArangoDB instances:
- _master_: this is the instance we'll activate the replication logger on
- _slave_: on this instance, we'll start a replication applier, and this will fetch data from the 
  master's events log and apply all events locally
  
For the following example setup, we'll use the instance *tcp://master.domain.org:8529* as the 
master, and the instance *tcp://slave.domain.org:8530* as a slave.

The goal is to have all data from the master *tcp://master.domain.org:8529* be replicated to 
the slave *tcp://slave.domain.org:8530*.

On the *master*, configure the replication logger to start automatically on ArangoDB startup.
Additionally, set some restrictions for the event log size:

    require("org/arangodb/replication").logger.properties({
      autoStart: true,
      maxEvents: 1048576,
      maxEventsSize: 0
    });

After that, start the replication logger on the master:

    require("org/arangodb/replication").logger.start();


On the *slave*, make sure there currently is no replication applier running:
    
    require("org/arangodb/replication").applier.stop();

After that, do an initial sync of the slave with data from the master. Execute the following
command on the slave:
    
    require("org/arangodb/replication").sync({
      endpoint: "tcp://master.example.org:8529",
      username: "myuser",
      password: "mypasswd"
    });

Warning: this will replace data on the slave with data from the master! Only execute the
command if you have verified you are on the correct server!

Then re-configure the slave's replication applier to point to the master, set the username and 
password for the connection, and set the `autoStart` attribute:
    
    require("org/arangodb/replication").applier.properties({
      endpoint: "tcp://master.example.org:8529",
      username: "myuser",
      password: "mypasswd",
      autoStart: true,
      adaptivePolling: true
    });

After that, start the applier on the slave:

    require("org/arangodb/replication").applier.start();

After that, you should be able to monitor the state and progress of the replication 
applier by executing the `state` command on the slave:
    
    require("org/arangodb/replication").applier.state();

You may also want to check the master and slave states via the HTTP APIs (see @ref
HttpReplication) or the web interface ("Dashboard" tab).

Replication Limitations {#UserManualReplicationLimitations}
===========================================================

The replication in ArangoDB 1.4-alpha has a few limitations still. Some of these 
limitations may be removed in later versions of ArangoDB:

- the event log on the master is currently written directly after a write operation
  is carried out on the master. In case the master crashes between having executed the
  write operation and having it written into the event log, the write operation may
  have been executed on the master, but may be lost for replication and not be applied
  on any slaves.
- there is no feedback from the slaves to the master. If a slave cannot apply an event
  it got from the master, the master will have a different state of data. In this 
  case, the replication applier on the slave will stop and report an error. Administrators
  can then either "fix" the problem or re-sync the data from the master to the slave
  and start the applier again.
- the replication is an asynchronous master-slave replication. There is currently no
  way to use it as a synchronous replication, or a multi-master replication.
- at the moment it is assumed that only the replication applier executes write 
  operations on a slave. ArangoDB currently does not prevent users from carrying out
  their own write operations on slaves, though this might lead to undefined behavior
  and the replication applier stopping.
- the replication logger will only log write operations for non-system collections. 
  Write operations for system collections are currently not logged, and thus will not
  be shipped to slaves.
- master servers do not know which slaves are or will be connected to them. All servers
  in a replication setup are currently only loosely coupled. There currently is no way 
  for a client to query which servers are present in a replication.
- failover must currently be handled by clients or client APIs.
- there currently is one replication logger and one replication applier per ArangoDB 
  database. It is thus not possible to have a slave apply the events logs from 
  multiple masters.
- the replication applier is single-threaded, but write operations on the master may
  be executed in parallel if they affect different collections. Thus the replication
  applier might not be able to catch up with a very powerful and loaded master.
- replication is only supported between ArangoDB 1.4 masters and 1.4 slaves. It is
  currently not possible to replicate from/to other ArangoDB versions.
- a replication applier cannot apply data from itself.

Replication Overhead {#UserManualReplicationOverhead}
=====================================================

Running the replication logger will make all data modification operations more 
expensive, as the ArangoDB server will write the operations into the replication log, too.

Additionally, replication appliers that connect to an ArangoDB master will cause some
extra work for the master as it needs to process incoming HTTP requests, and respond.

Overall, turning on the replication logger may reduce throughput on an ArangoDB master
by some extent. If the replication feature is not required, the replication logger should 
be turned off.

Transactions are logged to the event log on the master as an uninterrupted sequence.
While a transaction is written to the event log, the event log is blocked for other
writes. Transactions should thus be as small as possible.
