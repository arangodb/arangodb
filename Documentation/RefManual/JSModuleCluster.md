Module "planner"{#JSModuleCluster}
==================================

@NAVIGATE_JSModuleCluster
@EMBEDTOC{JSModuleClusterTOC}

Cluster Module{#JSModuleClusterIntro}
=====================================

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

@BNAVIGATE_JSModuleCluster
