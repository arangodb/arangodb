<a name="module_"planner""></a>
# Module "planner"

<a name="cluster_module"></a>
### Cluster Module

This module contains functions to plan, launch and shutdown clusters
of ArangoDB instances. We distinguish between the planning phase of
a cluster and the startup phase. The planning involves determining
how many processes in which role to run on which server, what ports and
endpoints to use, as well as the sequence of events to startup the whole
machinery. The result of such a planning phase is a "cluster plan". This
in turn can be given to a "Kickstarter", which uses the plan to actually
fire up the necessary processes. This is done via "dispatchers". A
dispatcher is nothing but a regular ArangoDB instance, compiled with the
cluster extensions, but not running in cluster mode. It exposes a REST
API to help in the planning and running of a cluster, mainly by
starting up further processes on the local machine. The planner needs
a complete description of all dispatchers in the system, which basically
describes the set of machines used for the cluster. These dispatchers
are also used during the planning to find free network ports.

Here are the details of the functionality:

`new require("org/arangodb/cluster").Planner(userConfig)`

This constructor builds a cluster planner object. The one and only argument is an object that can have the properties described below. The planner can plan clusters on a single machine (basically for testing purposes) and on multiple machines. The resulting "cluster plans" can be used by the kickstarter (see JSF_Cluster_Kickstarter_Constructor) to start up the processes comprising the cluster, including the agency. To this end, there has to be one dispatcher on every machine participating in the cluster. A dispatcher is a simple instance of ArangoDB, compiled with the cluster extensions, but not running in cluster mode. This is why the configuration option dispatchers below is of central importance.

* dispatchers: an object with a property for each dispatcher, the property name is the ID of the dispatcher and the value should be an object with at least the property endpoint containing the endpoint of the corresponding dispatcher. Further optional properties are:
	* avoidPorts which is an object in which all port numbers that should not be used are bound to true, default is empty, that is, all ports can be used
	* arangodExtraArgs, which is a list of additional command line arguments that will be given to DBservers and coordinators started by this dispatcher, the default is an empty list. These arguments will be appended to those produced automatically, such that one can overwrite things with this.
	* allowCoordinators, which is a boolean value indicating whether or not coordinators should be started on this dispatcher, the default is true
	* allowDBservers, which is a boolean value indicating whether or not DBservers should be started on this dispatcher, the default is true
	* allowAgents, which is a boolean value indicating whether or not agents should be started on this dispatcher, the default is true
	* username, which is a string that contains the user name for authentication with this dispatcher
	* passwd, which is a string that contains the password for authentication with this dispatcher, if not both username and passwd are set, then no authentication is used between dispatchers. Note that this will not work if the dispatchers are configured with authentication.
	* If .dispatchers is empty (no property), then an entry for the local arangod itself is automatically added. Note that if the only configured dispatcher has endpoint tcp://localhost:, all processes are started in a special "local" mode and are configured to bind their endpoints only to the localhost device. In all other cases both agents and arangod instances bind their endpoints to all available network devices.
* numberOfAgents: the number of agents in the agency, usually there is no reason to deviate from the default of 3. The planner distributes them amongst the dispatchers, if possible.
* agencyPrefix: a string that is used as prefix for all keys of configuration data stored in the agency.
* numberOfDBservers: the number of DBservers in the cluster. The planner distributes them evenly amongst the dispatchers.
* startSecondaries: a boolean flag indicating whether or not secondary servers are started. In this version, this flag is silently ignored, since we do not yet have secondary servers.
* numberOfCoordinators: the number of coordinators in the cluster, the planner distributes them evenly amongst the dispatchers.
* DBserverIDs: a list of DBserver IDs (strings). If the planner runs out of IDs it creates its own ones using DBserver concatenated with a unique number.
* coordinatorIDs: a list of coordinator IDs (strings). If the planner runs out of IDs it creates its own ones using Coordinator concatenated with a unique number.
* dataPath: this is a string and describes the path under which the agents, the DBservers and the coordinators store their data directories. This can either be an absolute path (in which case all machines in the clusters must use the same path), or it can be a relative path. In the latter case it is relative to the directory that is configured in the dispatcher with the cluster.data-path option (command line or configuration file). The directories created will be called data-PREFIX-ID where PREFIX is replaced with the agency prefix (see above) and ID is the ID of the DBserver or coordinator.
* logPath: this is a string and describes the path under which the DBservers and the coordinators store their log file. This can either be an absolute path (in which case all machines in the cluster must use the same path), or it can be a relative path. In the latter case it is relative to the directory that is configured in the dispatcher with the cluster.log-path option.
* arangodPath: this is a string and describes the path to the actual executable arangod that will be started for the DBservers and coordinators. If this is an absolute path, it obviously has to be the same on all machines in the cluster as described for dataPath. If it is an empty string, the dispatcher uses the executable that is configured with the cluster.arangod-path option, which is by default the same executable as the dispatcher uses.
* agentPath: this is a string and describes the path to the actual executable that will be started for the agents in the agency. If this is an absolute path, it obviously has to be the same on all machines in the cluster, as described for arangodPath. If it is an empty string, the dispatcher uses its cluster.agent-path option.
* agentExtPorts: a list of port numbers to use for the external ports of the agents. When running out of numbers in this list, the planner increments the last one used by one for every port needed. Note that the planner checks availability of the ports during the planning phase by contacting the dispatchers on the different machines, and uses only ports that are free during the planning phase. Obviously, if those ports are connected before the actual startup, things can go wrong.
* agentIntPorts: a list of port numbers to use for the internal ports of the agents. The same comments as for agentExtPorts apply.
* DBserverPorts: a list of port numbers to use for the DBservers. The same comments as for agentExtPorts apply.
* coordinatorPorts: a list of port numbers to use for the coordinators. The same comments as for agentExtPorts apply.
* useSSLonDBservers: a boolean flag indicating whether or not we use SSL on all DBservers in the cluster
* useSSLonCoordinators: a boolean falg indicating whether or not we use SSL on all coordinators in the cluster

All these values have default values. Here is the current set of default values:

	{
	  "agencyPrefix"            : "arango",
	  "numberOfAgents"          : 1,
	  "numberOfDBservers"       : 2,
	  "startSecondaries"        : false,
	  "numberOfCoordinators"    : 1,
	  "DBserverIDs"             : ["Pavel", "Perry", "Pancho", "Paul", "Pierre",
	                               "Pit", "Pia", "Pablo" ],
	  "coordinatorIDs"          : ["Claus", "Chantalle", "Claire", "Claudia",
	                               "Claas", "Clemens", "Chris" ],
	  "dataPath"                : "",   // means configured in dispatcher
	  "logPath"                 : "",   // means configured in dispatcher
	  "arangodPath"             : "",   // means configured as dispatcher
	  "agentPath"               : "",   // means configured in dispatcher
	  "agentExtPorts"           : [4001],
	  "agentIntPorts"           : [7001],
	  "DBserverPorts"           : [8629],
	  "coordinatorPorts"        : [8530],
	  "dispatchers"             : {"me": {"endpoint": "tcp://localhost:"}},
	                              // this means only we as a local instance
	  "useSSLonDBservers"       : false,
	  "useSSLonCoordinators"    : false
	};

`Planner.getPlan()`

returns the cluster plan as a JavaScript object. The result of this method can be given to the constructor of a Kickstarter.

`new require("org/arangodb/cluster").Kickstarter(plan)`

This constructor constructs a kickstarter object. Its first argument is a cluster plan as for example provided by the planner. The second argument is optional and is taken to be "me" if omitted, it is the ID of the dispatcher this object should consider itself to be. If the plan contains startup commands for the dispatcher with this ID, these commands are executed immediately. Otherwise they are handed over to another responsible dispatcher via a REST call.

The resulting object of this constructors allows to launch, shutdown, relaunch the cluster described in the plan.


`Kickstarter.launch()`

This starts up a cluster as described in the plan which was given to the constructor. To this end, other dispatchers are contacted as necessary. All startup commands for the local dispatcher are executed immediately.

The result is an object that contains information about the started processes, this object is also stored in the Kickstarter object itself. We do not go into details here about the data structure, but the most important information are the process IDs of the started processes. The corresponding shutdown method (see JSF_Kickstarter_prototype_shutdown) needs this information to shut down all processes.

Note that all data in the DBservers and all log files and all agency information in the cluster is deleted by this call. This is because it is intended to set up a cluster for the first time. See the relaunch method (see JSF_Kickstarter_prototype_relaunch) for restarting a cluster without data loss.

`Kickstarter.shutdown()`

This shuts down a cluster as described in the plan which was given to the constructor. To this end, other dispatchers are contacted as necessary. All processes in the cluster are gracefully shut down in the right order.

`Kickstarter.relaunch()`

This starts up a cluster as described in the plan which was given to the constructor. To this end, other dispatchers are contacted as necessary. All startup commands for the local dispatcher are executed immediately.

The result is an object that contains information about the started processes, this object is also stored in the Kickstarter object itself. We do not go into details here about the data structure, but the most important information are the process IDs of the started processes. The corresponding shutdown method (see JSF_Kickstarter_prototype_shutdown) needs this information to shut down all processes.

Note that this methods needs that all data in the DBservers and the agency information in the cluster are already set up properly. See the launch method (see JSF_Kickstarter_prototype_launch) for starting a cluster for the first time.

`Kickstarter.upgrade()`

This performs an upgrade procedure on a cluster as described in the plan which was given to the constructor. To this end, other dispatchers are contacted as necessary. All commands for the local dispatcher are executed immediately. The basic approach for the upgrade is as follows: The agency is started first (exactly as in relaunch), no configuration is sent there (exactly as in the relaunch action), all servers are first started with the option "&ndash;upgrade" and then normally. In the end, the version-check.js script is run on one of the coordinators, as in the launch action.

The result is an object that contains information about the started processes, this object is also stored in the Kickstarter object itself. We do not go into details here about the data structure, but the most important information are the process IDs of the started processes. The corresponding shutdown method (see JSF_Kickstarter_prototype_shutdown) needs this information to shut down all processes.

Note that this methods needs that all data in the DBservers and the agency information in the cluster are already set up properly. See the launch method (see JSF_Kickstarter_prototype_launch) for starting a cluster for the first time.


`Kickstarter.cleanup()`

This cleans up all the data and logs of a previously shut down cluster. To this end, other dispatchers are contacted as necessary. Use shutdown (see JSF_Kickstarter_prototype_shutdown) first and use with caution, since potentially a lot of data is being erased with this call!



<!--
@anchor JSModuleClusterPlannerConstructor
@copydetails JSF_Cluster_Planner_Constructor

@anchor JSModuleClusterPlannerGetPlan
@copydetails JSF_Planner_prototype_getPlan

@anchor JSModuleClusterKickstarterConstructor
@copydetails JSF_Cluster_Kickstarter_Constructor

@anchor JSModuleClusterKickstarterLaunch
@copydetails JSF_Kickstarter_prototype_launch

@anchor JSModuleClusterKickstarterShutdown
@copydetails JSF_Kickstarter_prototype_shutdown

@anchor JSModuleClusterKickstarterRelaunch
@copydetails JSF_Kickstarter_prototype_relaunch

@anchor JSModuleClusterKickstarterCleanup
@copydetails JSF_Kickstarter_prototype_cleanup

@BNAVIGATE_JSModuleCluster
-->