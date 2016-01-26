


`Kickstarter.upgrade(username, passwd)`

This performs an upgrade procedure on a cluster as described in
the plan which was given to the constructor. To this end, other
dispatchers are contacted as necessary. All commands for the local
dispatcher are executed immediately. The basic approach for the
upgrade is as follows: The agency is started first (exactly as
in relaunch), no configuration is sent there (exactly as in the
relaunch action), all servers are first started with the option
"--upgrade" and then normally. In the end, the upgrade-database.js
script is run on one of the coordinators, as in the launch action.

The result is an object that contains information about the started
processes, this object is also stored in the Kickstarter object
itself. We do not go into details here about the data structure,
but the most important information are the process IDs of the
started processes. The corresponding
[shutdown method](../ModulePlanner/README.md#shutdown) needs
this information to shut down all processes.

Note that this methods needs that all data in the DBservers and the
agency information in the cluster are already set up properly. See
the [launch method](../ModulePlanner/README.md#launch) for
starting a cluster for the first time.

