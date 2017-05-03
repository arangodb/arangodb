Components
==========

### Replication Logger

#### Purpose

The replication logger will write all data-modification operations into the write-ahead log.
This log may then be read by clients to replay any data modification on a different server.

#### Checking the state

To query the current state of the logger, use the *state* command:
  
    require("@arangodb/replication").logger.state();

The result might look like this:

```js
{ 
  "state" : { 
    "running" : true, 
      "lastLogTick" : "133322013", 
      "totalEvents" : 16, 
      "time" : "2014-07-06T12:58:11Z" 
  }, 
  "server" : { 
    "version" : "2.2.0-devel", 
    "serverId" : "40897075811372" 
  }, 
  "clients" : { 
  } 
}
```

The *running* attribute will always be true. In earlier versions of ArangoDB the replication was optional and this could have been *false*. 

The *totalEvents* attribute indicates how many log events have been logged since the start 
of the ArangoDB server. Finally, the *lastLogTick* value indicates the id of the last 
operation that was written to the server's write-ahead log. It can be used to determine whether new 
operations were logged, and is also used by the replication applier for incremental 
fetching of data.

**Note**: The replication logger state can also be queried via the
[HTTP API](../../../../HTTP/Replications/index.html).

To query which data ranges are still available for replication clients to fetch,
the logger provides the *firstTick* and *tickRanges* functions:

    require("@arangodb/replication").logger.firstTick();

This will return the minimum tick value that the server can provide to replication
clients via its replication APIs. The *tickRanges* function returns the minimum and
maximum tick values per logfile:

    require("@arangodb/replication").logger.tickRanges();

### Replication Applier

#### Purpose

The purpose of the replication applier is to read data from a master database's event log, 
and apply them locally. The applier will check the master database for new operations periodically. 
It will perform an incremental synchronization, i.e. only asking the master for operations
that occurred after the last synchronization.

The replication applier does not get notified by the master database when there are "new" 
operations available, but instead uses the pull principle. It might thus take some time (the 
so-called *replication lag*) before an operation from the master database gets shipped to and 
applied in a slave database. 

The replication applier of a database is run in a separate thread. It may encounter problems 
when an operation from the master cannot be applied safely, or when the connection to the master
database goes down (network outage, master database is down or unavailable etc.). In this case, 
the database's replication applier thread might terminate itself. It is then up to the 
administrator to fix the problem and restart the database's replication applier.

If the replication applier cannot connect to the master database, or the communication fails at
some point during the synchronization, the replication applier will try to reconnect to
the master database. It will give up reconnecting only after a configurable amount of connection 
attempts.

The replication applier state is queryable at any time by using the *state* command of the
applier. This will return the state of the applier of the current database:

```js
require("@arangodb/replication").applier.state();
```

The result might look like this:

```js
{ 
  "state" : { 
    "running" : true, 
    "lastAppliedContinuousTick" : "152786205", 
    "lastProcessedContinuousTick" : "152786205", 
    "lastAvailableContinuousTick" : "152786205", 
    "progress" : { 
      "time" : "2014-07-06T13:04:57Z", 
      "message" : "fetching master log from offset 152786205", 
      "failedConnects" : 0 
    }, 
    "totalRequests" : 38, 
    "totalFailedConnects" : 0, 
    "totalEvents" : 1, 
    "lastError" : { 
      "errorNum" : 0 
    }, 
    "time" : "2014-07-06T13:04:57Z" 
  }, 
  "server" : { 
    "version" : "2.2.0-devel", 
    "serverId" : "210189384542896" 
  }, 
  "endpoint" : "tcp://master.example.org:8529", 
  "database" : "_system" 
}
```

The *running* attribute indicates whether the replication applier of the current database
is currently running and polling the server at *endpoint* for new events.

The *progress.failedConnects* attribute shows how many failed connection attempts the replication
applier currently has encountered in a row. In contrast, the *totalFailedConnects* attribute
indicates how many failed connection attempts the applier has made in total. The
*totalRequests* attribute shows how many requests the applier has sent to the master database
in total. The *totalEvents* attribute shows how many log events the applier has read from the
master. 

The *progress.message* sub-attribute provides a brief hint of what the applier currently does 
(if it is running). The *lastError* attribute also has an optional *errorMessage* sub-attribute, 
showing the latest error message. The *errorNum* sub-attribute of the *lastError* attribute can be 
used by clients to programmatically check for errors. It should be *0* if there is no error, and 
it should be non-zero if the applier terminated itself due to a problem.

Here is an example of the state after the replication applier terminated itself due to 
(repeated) connection problems:

```js
{ 
  "state" : { 
    "running" : false, 
    "progress" : { 
      "time" : "2014-07-06T13:14:37Z", 
      "message" : "applier stopped", 
      "failedConnects" : 6 
    }, 
    "totalRequests" : 79, 
    "totalFailedConnects" : 11, 
    "totalEvents" : 0, 
    "lastError" : { 
      "time" : "2014-07-06T13:09:41Z", 
      "errorMessage" : "could not connect to master at tcp://master.example.org:8529: Could not connect to 'tcp:/...", 
      "errorNum" : 1400 
    },
    ...
  }
}
```

**Note**: the state of a database's replication applier is queryable via the HTTP API, too. 
Please refer to [HTTP Interface for Replication](../../../../HTTP/Replications/index.html) for more details.

#### All-in-one setup

To copy the initial data from the **slave** to the master and start the
continuous replication, there is an all-in-one command *setupReplication*:

```js
require("@arangodb/replication").setupReplication(configuration);
```

The following example demonstrates how to use the command for setting up replication
for the *_system* database. Note that it should be run on the slave and not the master:

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
is started, or in case the initial synchronization has failed. 

If the initial synchronization is successful, the command will store the given configuration on
the slave. It also configures the continuous replication to start automatically if the slave is 
restarted, i.e. *autoStart* is set to *true*.

If the command is run while the slave's replication applier is already running, it will first
stop the running applier, drop its configuration and do a resynchronization of data with the
master. It will then use the provided configration, overwriting any previously existing replication
configuration on the slave.

#### Starting and Stopping

To manually start and stop the applier in the current database, the *start* and *stop* commands 
can be used like this:

```js
require("@arangodb/replication").applier.start(<tick>);
require("@arangodb/replication").applier.stop();
```

**Note**: Starting a replication applier without setting up an initial configuration will 
fail. The replication applier will look for its configuration in a file named 
*REPLICATION-APPLIER-CONFIG* in the current database's directory. If the file is not present, 
ArangoDB will use some default configuration, but it cannot guess the endpoint (the address 
of the master database) the applier should connect to. Thus starting the applier without 
configuration will fail.

Note that at the first time you start the applier, you should pass the value returned in the
*lastLogTick* attribute of the initial sync operation.

**Note**: Starting a database's replication applier via the *start* command will not necessarily 
start the applier on the next and following ArangoDB server restarts. Additionally, stopping a
database's replication applier manually will not necessarily prevent the applier from being 
started again on the next server start. All of this is configurable separately (hang on reading).

**Note**: when stopping and restarting the replication applier of database, it will resume where 
it last stopped. This is sensible because replication log events should be applied incrementally. 
If the replication applier of a database has never been started before, it needs some *tick* value 
from the master's log from which to start fetching events.

There is one caveat to consider when stopping a replication on the slave: if there are still
ongoing replicated transactions that are neither committed or aborted, stopping the replication
applier will cause these operations to be lost for the slave. If these transactions commit on the
master later and the replication is resumed, the slave will not be able to commit these transactions,
too. Thus stopping the replication applier on the slave manually should only be done if there
is certainty that there are no ongoing transactions on the master.


#### Configuration

To configure the replication applier of a specific database, use the *properties* command. Using 
it without any arguments will return the applier's current configuration:

```js
require("@arangodb/replication").applier.properties();
```

The result might look like this:

```js
{ 
  "requestTimeout" : 600, 
  "connectTimeout" : 10, 
  "ignoreErrors" : 0, 
  "maxConnectRetries" : 10, 
  "chunkSize" : 0, 
  "autoStart" : false, 
  "adaptivePolling" : true,
  "includeSystem" : true,
  "requireFromPresent" : false,
  "autoResync" : false,
  "autoResyncRetries" : 2,
  "verbose" : false 
}
```

**Note**: There is no *endpoint* attribute configured yet. The *endpoint* attribute is required
for the replication applier to be startable. You may also want to configure a username and password
for the connection via the *username* and *password* attributes. 

```js
require("@arangodb/replication").applier.properties({ 
  endpoint: "tcp://master.domain.org:8529", 
  username:  "root", 
  password: "secret",
  verbose: false
});
```

This will re-configure the replication applier for the current database. The configuration will be 
used from the next start of the replication applier. The replication applier cannot be re-configured 
while it is running. It must be stopped first to be re-configured.

To make the replication applier of the current database start automatically when the ArangoDB server 
starts, use the *autoStart* attribute. 

Setting the *adaptivePolling* attribute to *true* will make the replication applier poll the 
master database for changes with a variable frequency. The replication applier will then lower the 
frequency when the master is idle, and increase it when the master can provide new events).
Otherwise the replication applier will poll the master database for changes with a constant frequency.

The *idleMinWaitTime* attribute controls the minimum wait time (in seconds) that the replication applier 
will intentionally idle before fetching more log data from the master in case the master has already 
sent all its log data. This wait time can be used to control the frequency with which the replication 
applier sends HTTP log fetch requests to the master in case there is no write activity on the master.

The *idleMaxWaitTime* attribute controls the maximum wait time (in seconds) that the replication 
applier will intentionally idle before fetching more log data from the master in case the master has 
already sent all its log data and there have been previous log fetch attempts that resulted in no more 
log data. This wait time can be used to control the maximum frequency with which the replication 
applier sends HTTP log fetch requests to the master in case there is no write activity on the master 
for longer periods. Note that this configuration value will only be used if the option *adaptivePolling* 
is set to *true*.

To set a timeout for connection and following request attempts, use the *connectTimeout* and 
*requestTimeout* values. The *maxConnectRetries* attribute configures after how many failed 
connection attempts in a row the replication applier will give up and turn itself off. 
You may want to set this to a high value so that temporary network outages do not lead to the 
replication applier stopping itself.
The *connectRetryWaitTime* attribute configures how long the replication applier will wait
before retrying the connection to the master in case of connection problems.

The *chunkSize* attribute can be used to control the approximate maximum size of a master's
response (in bytes). Setting it to a low value may make the master respond faster (less data is
assembled before the master sends the response), but may require more request-response roundtrips.
Set it to *0* to use ArangoDB's built-in default value.

The *includeSystem* attribute controls whether changes to system collections (such as *_graphs* or
*_users*) should be applied. If set to *true*, changes in these collections will be replicated, 
otherwise, they will not be replicated. It is often not necessary to replicate data from system
collections, especially because it may lead to confusion on the slave because the slave needs to 
have its own system collections in order to start and keep operational.

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
log data the slave requests, the replication applier will stop as usual. But due to the fact
that *autoResync* is set to true, the slave will automatically trigger a full resync of all data with 
the master. After that, the replication applier will go into continuous replication mode again.
Additionally, setting *autoResync* to *true* will trigger a full re-synchronization of data when
the continuous replication is started and detects that there is no start tick value.

Automatic re-synchronization may transfer a lot of data from the master to the slave and can be 
expensive. It is therefore turned off by default. When turned off, the slave will never perform an 
automatic re-synchronization with the master.

The *autoResyncRetries* option can be used to control the number of resynchronization retries that 
will be performed in a row when automatic resynchronization is enabled and kicks in. Setting this to 
*0* will effectively disable *autoResync*. Setting it to some other value will limit the number of retries 
that are performed. This helps preventing endless retries in case resynchronizations always fail.

The *verbose* attribute controls the verbosity of the replication logger. Setting it to `true` will
make the replication applier write a line to the log for every operation it performs. This should
only be used for diagnosing replication problems.

The following example will set most of the discussed properties for the current database's applier:

```js
require("@arangodb/replication").applier.properties({ 
  endpoint: "tcp://master.domain.org:8529", 
  username: "root", 
  password: "secret",
  adaptivePolling: true,
  connectTimeout: 15,
  maxConnectRetries: 100,
  chunkSize: 262144,
  autoStart: true,
  includeSystem: true,
  autoResync: true,
  autoResyncRetries: 2,
});
```

After the applier is now fully configured, it could theoretically be started. However, we
may first need an initial synchronization of all collections and their data from the master before
we start the replication applier. 

The only safe method for doing a full synchronization (or re-synchronization) is thus to 

* stop the replication applier on the slave (if currently running)
* perform an initial full sync with the master database
* note the master database's *lastLogTick* value and
* start the continuous replication applier on the slave using this tick value.

The initial synchronization for the current database is executed with the *sync* command:

```js
require("@arangodb/replication").sync({
  endpoint: "tcp://master.domain.org:8529",
  username: "root",
  password: "secret,
  includeSystem: true
});
```

The *includeSystem* option controls whether data from system collections (such as *_graphs* and
*_users*) shall be synchronized. 

The initial synchronization can optionally be configured to include or exclude specific 
collections using the *restrictType* and *restrictCollection* parameters.

The following command only synchronizes collection *foo* and *bar*:

```js
require("@arangodb/replication").sync({
  endpoint: "tcp://master.domain.org:8529",
  username: "root",
  password: "secret,
  restrictType: "include",
  restrictCollections: [ "foo", "bar" ]
});
```

Using a *restrictType* of *exclude*, all collections but the specified will be synchronized.

**Warning**: *sync* will do a full synchronization of the collections in the current database with
collections present in the master database.
Any local instances of the collections and all their data are removed! Only execute this
command if you are sure you want to remove the local data!

As *sync* does a full synchronization, it might take a while to execute.
When *sync* completes successfully, it returns an array of collections it has synchronized in its
*collections* attribute. It will also return the master database's last log tick value 
at the time the *sync* was started on the master. The tick value is contained in the *lastLogTick*
attribute of the *sync* command: 

```js
{ 
  "lastLogTick" : "231848833079705", 
  "collections" : [ ... ]
}
```
Now you can start the continuous synchronization for the current database on the slave
with the command

```js
require("@arangodb/replication").applier.start("231848833079705");
```

**Note**: The tick values should be treated as strings. Using numeric data types for tick
values is unsafe because they might exceed the 32 bit value and the IEEE754 double accuracy
ranges.

