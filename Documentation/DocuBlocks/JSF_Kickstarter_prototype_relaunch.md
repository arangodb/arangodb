


`Kickstarter.relaunch()`

This starts up a cluster as described in the plan which was given to
the constructor. To this end, other dispatchers are contacted as
necessary. All startup commands for the local dispatcher are
executed immediately.

The result is an object that contains information about the started
processes, this object is also stored in the Kickstarter object
itself. We do not go into details here about the data structure,
but the most important information are the process IDs of the
started processes. The corresponding
[shutdown method ](../ModulePlanner/README.md#shutdown) needs this information to
shut down all processes.

Note that this methods needs that all data in the DBservers and the
agency information in the cluster are already set up properly. See
the [launch method](../ModulePlanner/README.md#launch) for
starting a cluster for the first time.

