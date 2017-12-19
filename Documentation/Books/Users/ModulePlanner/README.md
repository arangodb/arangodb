Module "planner"
================

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

### Require
<!-- js/server/modules/org/arangodb/cluster/planner.js -->
@startDocuBlock JSF_Cluster_Planner_Constructor

### Get Plan
<!-- js/server/modules/org/arangodb/cluster/planner.js -->
@startDocuBlock JSF_Planner_prototype_getPlan

### Require
<!-- js/server/modules/org/arangodb/cluster/kickstarter.js -->
@startDocuBlock JSF_Cluster_Kickstarter_Constructor

### Launch
<!-- js/server/modules/org/arangodb/cluster/kickstarter.js -->
@startDocuBlock JSF_Kickstarter_prototype_launch

### Check Cluster Health
@startDocuBlock JSF_Kickstarter_prototype_isHealthy

### Shutdown
<!-- js/server/modules/org/arangodb/cluster/kickstarter.js -->
@startDocuBlock JSF_Kickstarter_prototype_shutdown

### Relaunch
<!-- js/server/modules/org/arangodb/cluster/kickstarter.js -->
@startDocuBlock JSF_Kickstarter_prototype_relaunch

### Upgrade
<!-- js/server/modules/org/arangodb/cluster/kickstarter.js -->
@startDocuBlock JSF_Kickstarter_prototype_upgrade

### Cleanup
<!-- js/server/modules/org/arangodb/cluster/kickstarter.js -->
@startDocuBlock JSF_Kickstarter_prototype_cleanup

