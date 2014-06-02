!CHAPTER Example Setup

Setting up a working master-slave replication requires two ArangoDB instances:
- _master_: this is the instance we'll activate the replication logger on
- _slave_: on this instance, we'll start a replication applier, and this will fetch data from the 
  master database's events log and apply all events locally
  
For the following example setup, we'll use the instance *tcp://master.domain.org:8529* as the 
master, and the instance *tcp://slave.domain.org:8530* as a slave.

The goal is to have all data from the database `_system` on master *tcp://master.domain.org:8529* 
be replicated to the database `_system` on the slave *tcp://slave.domain.org:8530*.

On the *master*, configure the replication logger for database `_system` to start automatically 
on ArangoDB startup. Additionally, set some restrictions for the event log size:

    db._useDatabase("_system");

    require("org/arangodb/replication").logger.properties({
      autoStart: true,
      maxEvents: 1048576,
      maxEventsSize: 0
    });

After that, start the replication logger for the `_system` database on the master:

    db._useDatabase("_system");
    require("org/arangodb/replication").logger.start();


On the *slave*, make sure there currently is no replication applier running in the `_system`
database:

    db._useDatabase("_system");
    require("org/arangodb/replication").applier.stop();

After that, do an initial sync of the slave with data from the master. Execute the following
commands on the slave:
    
    db._useDatabase("_system");
    require("org/arangodb/replication").sync({
      endpoint: "tcp://master.example.org:8529",
      username: "myuser",
      password: "mypasswd"
    });

Warning: this will replace data in the slave database with data from the master database! 
Only execute the commands if you have verified you are on the correct server, in the
correct database!

Then re-configure the slave database's replication applier to point to the master database, 
set the username and password for the connection, and set the `autoStart` attribute:
    
    db._useDatabase("_system");
    require("org/arangodb/replication").applier.properties({
      endpoint: "tcp://master.example.org:8529",
      username: "myuser",
      password: "mypasswd",
      autoStart: true,
      adaptivePolling: true
    });

After that, start the applier in the slave database:

    db._useDatabase("_system");
    require("org/arangodb/replication").applier.start();

After that, you should be able to monitor the state and progress of the replication 
applier by executing the `state` command in the slave database:
    
    db._useDatabase("_system");
    require("org/arangodb/replication").applier.state();

You may also want to check the master and slave states via the HTTP APIs (see [HTTP Interface for Replication](../HttpReplications/README.md)) or the web interface ("Dashboard" tab).

