Example Setup
=============

Setting up a working master-slave replication requires two ArangoDB instances:
* **master**: this is the instance that all data-modification operations should be directed to
* **slave**: on this instance, we'll start a replication applier, and this will fetch data from the 
  master database's write-ahead log and apply its operations locally
  
For the following example setup, we'll use the instance *tcp://master.domain.org:8529* as the 
master, and the instance *tcp://slave.domain.org:8530* as a slave.

The goal is to have all data from the database *_system* on master *tcp://master.domain.org:8529* 
be replicated to the database *_system* on the slave *tcp://slave.domain.org:8530*.

On the **master**, nothing special needs to be done, as all write operations will automatically be
logged in the master's write-ahead log (WAL).

All-in-one setup
----------------

To make the replication copy the initial data from the **master** to the **slave** and start the
continuous replication on the **slave**, there is an all-in-one command:

```js
require("@arangodb/replication").setupReplication(configuration);
```

The following example demonstrates how to use the command for setting up replication
for the *_system* database. Note that it should be run on the **slave** and not the
**master**:

```js
db._useDatabase("_system");
require("@arangodb/replication").setupReplication({
  endpoint: "tcp://master.domain.org:8529",
  username: "myuser",
  password: "mypasswd",
  verbose: false,
  includeSystem: false,
  incremental: true,
  autoResync: true
});
```

The command will return when the initial synchronization is finished and the continuous replication
has been started, or in case the initial synchronization has failed. 

If the initial synchronization is successful, the command will store the given configuration on
the slave. It also configures the continuous replication to start automatically if the slave is 
restarted, i.e. *autoStart* is set to *true*.

If the command is run while the slave's replication applier is already running, it will first
stop the running applier, drop its configuration and do a resynchronization of data with the
**master**. It will then use the provided configration, overwriting any previously existing replication
configuration on the **slave**.


Initial synchronization
-----------------------

The initial synchronization and continuous replication applier can also be started separately.
To start replication on the **slave**, make sure there currently is no replication applier running.

The following commands stop a running applier in the slave's *_system* database:

```js
db._useDatabase("_system");
require("@arangodb/replication").applier.stop();
```

The *stop* operation will terminate any replication activity in the _system database on the slave.


After that, the initial synchronization can be run. It will copy the collections from the **master**
to the **slave**, overwriting existing data. To run the initial synchronization, execute the following 
commands on the **slave**:

```js
db._useDatabase("_system");
require("@arangodb/replication").sync({
  endpoint: "tcp://master.domain.org:8529",
  username: "myuser",
  password: "mypasswd",
  verbose: false
});
```

Username and password only need to be specified when the **master** requires authentication.
To check what the synchronization is currently doing, supply set the *verbose* option to *true*.
If set, the synchronization will create log messages with the current synchronization status.

**Warning**: The *sync* command will replace data in the **slave** database with data from the 
**master** database! Only execute these commands if you have verified you are on the correct server, 
in the correct database!

The sync operation will return an attribute named *lastLogTick* which we'll need to note. The
last log tick will be used as the starting point for subsequent replication activity. Let's
assume we got the following last log tick:

```js
{ 
  "lastLogTick" : "40694126", 
  ...
}
```

Initial synchronization from the ArangoShell
--------------------------------------------

The initial synchronization via the *sync* command may take a long time to complete. The shell
will block until the slave has completed the initial synchronization or until an error occurs.
By default, the *sync* command in the ArangoShell will poll the slave for a status update every
10 seconds.

Optionally the *sync* command can be made non-blocking by setting its *async* option to true.
In this case, the *sync command* will return instantly with an id string, and the initial 
synchronization will run detached on the master. To fetch the current status of the *sync* 
progress from the ArangoShell, the *getSyncResult* function can be used as follows:

```js
db._useDatabase("_system");
var replication = require("@arangodb/replication");

/* run command in async mode */
var id = replication.sync({
  endpoint: "tcp://master.domain.org:8529",
  username: "myuser",
  password: "mypasswd",
  async: true
});

/* now query the status of our operation */
print(replication.getSyncResult(id));
```

*getSyncResult* will return *false* as long as the synchronization is not complete, and return the
synchronization result otherwise.


Continuous synchronization
--------------------------

When the initial synchronization is finished, the continuous replication applier can be started using
the last log tick provided by the *sync* command. Before starting it, there is at least one 
configuration option to consider: replication on the **slave** will be running until the 
**slave** gets shut down. When the slave server gets restarted, replication will be turned off again. 
To change this, we first need to configure the slave's replication applier and set its
*autoStart* attribute.

Here's the command to configure the replication applier with several options, including the
*autoStart* attribute:

```js
db._useDatabase("_system");
require("@arangodb/replication").applier.properties({
  endpoint: "tcp://master.domain.org:8529",
  username: "myuser",
  password: "mypasswd",
  autoStart: true,
  autoResync: true,
  autoResyncRetries: 2,
  adaptivePolling: true,
  includeSystem: false,
  requireFromPresent: false,
  idleMinWaitTime: 0.5,
  idleMaxWaitTime: 1.5,
  verbose: false
});
```

An important consideration for replication is whether data from system collections (such as 
*_graphs* or *_users*) should be applied. The *includeSystem* option controls that. If set to 
*true*, changes in system collections will be replicated. Otherwise, they will not be replicated. 
It is often not necessary to replicate data from system collections, especially because it may 
lead to confusion on the slave because the slave needs to have its own system collections in 
order to start and keep operational.

The *requireFromPresent* attribute controls whether the applier will start synchronizing in case
it detects that the master cannot provide data for the initial tick value provided by the slave. 
This may be the case if the master does not have a big enough backlog of historic WAL logfiles,
and when the replication is re-started after a longer pause. When *requireFromPresent* is set to 
*true*, then the replication applier will check at start whether the start tick from which it starts 
or resumes replication is still present on the master. If not, then there would be data loss. If 
*requireFromPresent* is *true*, the replication applier will abort with an appropriate error message. 
If set to *false*, then the replication applier will still start, and ignore the data loss.

The *autoResync* option can be used in conjunction with the *requireFromPresent* option as follows:
when both *requireFromPresent* and *autoResync* are set to *true* and the master cannot provide the 
log data the slave had requested, the replication applier will stop as usual. But due to the fact
that *autoResync* is set to true, the slave will automatically trigger a full resync of all data with 
the master. After that, the replication applier will go into continuous replication mode again.
Additionally, setting *autoResync* to *true* will trigger a full re-synchronization of data when
the continuous replication is started and detects that there is no start tick value.

Note that automatic re-synchronization (*autoResync* option set to *true*) may transfer a lot of 
data from the master to the slave and can therefore be expensive. Still it's turned on here so
there's less need for manual intervention. 

The *autoResyncRetries* option can be used to control the number of resynchronization retries that 
will be performed in a row when automatic resynchronization is enabled and kicks in. Setting this to 
*0* will effectively disable *autoResync*. Setting it to some other value will limit the number of retries 
that are performed. This helps preventing endless retries in case resynchronizations always fail.

Now it's time to start the replication applier on the slave using the last log tick we got
before:

```js
db._useDatabase("_system");
require("@arangodb/replication").applier.start("40694126");
```

This will replicate all operations happening in the master's system database and apply them
on the slave, too.

After that, you should be able to monitor the state and progress of the replication 
applier by executing the *state* command on the slave server:

```js
    db._useDatabase("_system");
    require("@arangodb/replication").applier.state();
```

Please note that stopping the replication applier on the slave using the *stop* command 
should be avoided. The reason is that currently ongoing transactions (that have partly been
replicated to the slave) will be need to be restarted after a restart of the replication
applier. Stopping and restarting the replication applier on the slave should thus only be 
performed if there is certainty that the master is currently fully idle and all transactions 
have been replicated fully.

Note that while a slave has only partly executed a transaction from the master, it might keep
a write lock on the collections involved in the transaction.

You may also want to check the master and slave states via the HTTP APIs 
(see [HTTP Interface for Replication](../../../../HTTP/Replications/index.html)).

