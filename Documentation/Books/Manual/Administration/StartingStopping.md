Starting & Stopping ArangoDB
============================

This section can be used as a quick reference for the instructions on how to start and stop ArangoDB.

The exact way on how to start and stop ArangoDB depends on several factors, including the operating system in use, and how ArangoDB has been deployed.


Starting ArangoDB
-----------------

### Single Instance

#### Linux

This is what the linux installation page included:


After installation, the ArangoDB Server can be started with a command like the
following:

    unix> /etc/init.d/arangod start
 
The above command will start the server, and do that as well at system boot time.

To stop the server you can use the following command:

    unix> /etc/init.d/arangod stop

The exact commands depend on your Linux distribution.
You may require root privileges to execute these commands.


Also this was in the linux install page under 'Non-Standard Installation' section:

If you compiled ArangoDB from source and did not use any installation
package – or using non-default locations and/or multiple ArangoDB
instances on the same host – you may want to start the server process 
manually. You can do so by invoking the arangod binary from the command
line as shown below:

```
unix> /usr/local/sbin/arangod /tmp/vocbase
20ZZ-XX-YYT12:37:08Z [8145] INFO using built-in JavaScript startup files
20ZZ-XX-YYT12:37:08Z [8145] INFO ArangoDB (version 1.x.y) is ready for business
20ZZ-XX-YYT12:37:08Z [8145] INFO Have Fun!
```

To stop the database server gracefully, you can
either press CTRL-C or by send the SIGINT signal to the server process. 
On many systems this can be achieved with the following command:

    unix> kill -2 `pidof arangod`


Once you started the server, there should be a running instance of *_arangod_* -
the ArangoDB database server.

    unix> ps auxw | fgrep arangod
    arangodb 14536 0.1 0.6 5307264 23464 s002 S 1:21pm 0:00.18 /usr/local/sbin/arangod

If there is no such process, check the log file
*/var/log/arangodb/arangod.log* for errors. If you see a log message like

    2012-12-03T11:35:29Z [12882] ERROR Database directory version (1) is lower than server version (1.2).
    2012-12-03T11:35:29Z [12882] ERROR It seems like you have upgraded the ArangoDB binary. If this is what you wanted to do, please restart with the --database.auto-upgrade option to upgrade the data in the database directory.
    2012-12-03T11:35:29Z [12882] FATAL Database version check failed. Please start the server with the --database.auto-upgrade option

make sure to start the server once with the *--database.auto-upgrade* option.

Note that you may have to enable logging first. If you start the server
in a shell, you should see errors logged there as well.



#### Mac

#### Windows

##### Installer

##### XCopy Installation

This was on the xcopy file:


To start the database simply run it:

```
C:\Program Files\ArangoDB-3.1.11>usr\bin\arangod
```

Once the server is ready the output will be similar to the following:

```
INFO ArangoDB (version 3.1.11 [windows]) is ready for business. Have fun!
```

Now you can open the administrative webinterface in your browser using http://127.0.0.1:8529/.

#### Installing as service

If you don't want to run `arangod` from a cmd-shell each time installing it as a system service is the right thing to do.
This requires administrative privileges. You need to *Run as Administrator* the cmd-shell.
First we need to grant the SYSTEM-user access to our database directory, since `arangod` is going to be running as that user:

```
C:\Program Files\ArangoDB-3.1.11>icacls var /grant SYSTEM:F /t
```

Next we can install the service itself:

```
C:\Program Files\ArangoDB-3.1.11>usr\bin\arangod --install-service
```

Now you will have a new entry in the **Services** dialog labeled **ArangoDB - the multi-purpose database**. You can start it there or just do it on the `commandline` using:

```
C:\Program Files\ArangoDB-3.1.11>NET START ArangoDB
```

It will take a similar amount of time to start from the `comandline` above till the service is up and running.
Since you don't have any console to inspect the startup, messages of the severity FATAL & ERROR are also output into the windows eventlog, so in case of failure you can have a look at the **Eventlog** in the **Managementconsole**




### Master-Slave




### Active Failover

### Cluster

### DC2DC



Stopping ArangoDB
-----------------


### Single Instance

#### Linux

#### Mac

#### Windows


##### Installer

##### XCopy Installation



### Master-Slave




### Active Failover

To stop an _Active Failover_ it is necessary to stop all its processes on all the machines in use.

As a general guideline, it is recommended to stop the _followers_ before the _leader_.

#### Starter Deployments

If you have deployed using the ArangoDB _Starter_, on each machines you can press CTR-C or send a _SIGTERM_ signal (_kill -15_) to the _arangodb_ process.

Note: all _arangod_ processes that were started by the _arangodb_ process that is being stopped will be stopped as well. 

In case you are starting the _Starter_ using using _systemd_, on each machine, please execute a command similar to the following:

```
sudo systemctl stop _name-of-your-arangodb-service_.
```

#### Manual Deployments

If you have deployed your _Active Failover_ manually, you will need to stop all the _arangod_ processes on all machines that are part of that specific _Active Failover_ setup. This can be done by pressing _CTR-C_ or sending a _SIGTERM_ signal (_kill -15_) to the _arangod_ processes.

When stopping a manual deployment, it is suggested to first stop all the _follover_ processes, then the _leader_ process, and then all the _agent_ processes.



### Cluster

To stop a _Cluster_ it is necessary to stop all its processes on all the machines in use.

As a general guideline, it is recommended to stop all the processes (almost) simultaneously.

#### Starter Deployments

If you have deployed using the ArangoDB _Starter_, on each machines you can press CTR-C or send a _SIGTERM_ signal (_kill -15_) to the _arangodb_ process.

Note: all _arangod_ processes that were started by the _arangodb_ process that is being stopped will be stopped as well. 

In case you are starting the _Starter_ using using _systemd_, on each machine, please execute a command similar to the following:

```
sudo systemctl stop _name-of-your-arangodb-service_.
```

#### Manual Deployments

If you have deployed your _Cluster_ manually, you will need to stop all the _arangod_ processes on all machines that are part of that specific _Cluster_ setup. This can be done by pressing _CTR-C_ or sending a _SIGTERM_ signal (_kill -15_) to the _arangod_ processes.


### DC2DC