Starting & Stopping ArangoDB
============================

This section can be used as a quick reference for the instructions on how to start and stop ArangoDB.

The exact way on how to start and stop ArangoDB depends on several factors, including the operating system in use, and how ArangoDB has been deployed.


Starting ArangoDB
-----------------

### Single Instance

#### Linux

#### Mac

#### Windows

##### Installer

##### XCopy Installation


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