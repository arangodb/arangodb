Master/Slave Architecture
=========================

Introduction
------------

In a _Master/Slave_ setup one or more ArangoDB _slaves_ asynchronously replicate
from a _master_. 

The _master_ is the ArangoDB instance where all data-modification operations should
be directed to. The _slave_ is the ArangoDB instance that replicates the data from
the master.

Components
----------

### Replication Logger

**Purpose**

The _replication logger_ will write all data-modification operations into the
_write-ahead log_. This log may then be read by clients to replay any data
modification on a different server.

**Checking the state**

To query the current state of the _logger_, use the *state* command:
  
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

The *running* attribute will always be true. In earlier versions of ArangoDB the
replication was optional and this could have been *false*. 

The *totalEvents* attribute indicates how many log events have been logged since
the start of the ArangoDB server. Finally, the *lastLogTick* value indicates the
_id_ of the last operation that was written to the server's _write-ahead log_.
It can be used to determine whether new operations were logged, and is also used
by the _replication applier_ for incremental fetching of data.

**Note**: The replication logger state can also be queried via the
[HTTP API](../../../../HTTP/Replications/index.html).

To query which data ranges are still available for replication clients to fetch,
the logger provides the *firstTick* and *tickRanges* functions:

    require("@arangodb/replication").logger.firstTick();

This will return the minimum tick value that the server can provide to replication
clients via its replication APIs. The *tickRanges* function returns the minimum
and maximum tick values per logfile:

    require("@arangodb/replication").logger.tickRanges();

	
### Replication Applier

**Purpose**

The purpose of the _replication applier_ is to read data from a master database's
event log, and apply them locally. The _applier_ will check the master database
for new operations periodically. It will perform an incremental synchronization,
i.e. only asking the master for operations that occurred after the last synchronization.

The _replication applier_ does not get notified by the master database when there
are "new" operations available, but instead uses the pull principle. It might thus
take some time (the so-called *replication lag*) before an operation from the master
database gets shipped to, and applied in, a slave database. 

The _replication applier_ of a database is run in a separate thread. It may encounter
problems when an operation from the master cannot be applied safely, or when the
connection to the master database goes down (network outage, master database is
down or unavailable etc.). In this case, the database's _replication applier_ thread
might terminate itself. It is then up to the administrator to fix the problem and
restart the database's _replication applier_.

If the _replication applier_ cannot connect to the master database, or the
communication fails at some point during the synchronization, the _replication applier_
will try to reconnect to the master database. It will give up reconnecting only
after a configurable amount of connection attempts.

The _replication applier_ state is queryable at any time by using the *state* command
of the _applier_. This will return the state of the _applier_ of the current database:

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

The *running* attribute indicates whether the _replication applier_ of the current
database is currently running and polling the server at *endpoint* for new events.

The *progress.failedConnects* attribute shows how many failed connection attempts
the _replication applier_ currently has encountered in a row. In contrast, the
*totalFailedConnects* attribute indicates how many failed connection attempts the
_applier_ has made in total. The *totalRequests* attribute shows how many requests
the _applier_ has sent to the master database in total. The *totalEvents* attribute
shows how many log events the _applier_ has read from the master. 

The *progress.message* sub-attribute provides a brief hint of what the _applier_
currently does (if it is running). The *lastError* attribute also has an optional
*errorMessage* sub-attribute, showing the latest error message. The *errorNum*
sub-attribute of the *lastError* attribute can be used by clients to programmatically
check for errors. It should be *0* if there is no error, and it should be non-zero
if the _applier_ terminated itself due to a problem.

Below is an example of the state after the _replication applier_ terminated itself
due to (repeated) connection problems:

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

**Note**: the state of a database's replication applier is queryable via the HTTP
API, too. Please refer to [HTTP Interface for Replication](../../../../HTTP/Replications/index.html)
for more details.

