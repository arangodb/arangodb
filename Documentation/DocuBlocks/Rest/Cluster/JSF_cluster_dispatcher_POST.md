
@startDocuBlock JSF_cluster_dispatcher_POST
@brief exposes the dispatcher functionality to start up, shutdown,
relaunch, upgrade or cleanup a cluster according to a cluster plan
as for example provided by the kickstarter.

@RESTHEADER{POST /_admin/clusterDispatch,execute startup commands}

@RESTQUERYPARAMETERS

@RESTBODYPARAM{clusterPlan,object,required,}
is a cluster plan (see JSF_cluster_planner_POST),

@RESTBODYPARAM{myname,string,required,string}
is the ID of this dispatcher, this is used to decide
which commands are executed locally and which are forwarded
to other dispatchers

@RESTBODYPARAM{action,string,required,string}
can be one of the following:
    - "launch": the cluster is launched for the first time, all
      data directories and log files are cleaned and created
    - "shutdown": the cluster is shut down, the additional property
      *runInfo* (see below) must be bound as well
    - "relaunch": the cluster is launched again, all data directories
      and log files are untouched and need to be there already
    - "cleanup": use this after a shutdown to remove all data in the
      data directories and all log files, use with caution
    - "isHealthy": checks whether or not the processes involved
      in the cluster are running or not. The additional property
      *runInfo* (see above) must be bound as well
    - "upgrade": performs an upgrade of a cluster, to this end,
      the agency is started, and then every server is once started
      with the "--upgrade" option, and then normally. Finally,
      the script "verion-check.js" is run on one of the coordinators
      for the cluster.

@RESTBODYPARAM{runInfo,object,optional,}
this is needed for the "shutdown" and "isHealthy" actions
only and should be the structure that "launch", "relaunch" or
"upgrade" returned. It contains runtime information like process
IDs.

@RESTDESCRIPTION
The body must be an object with the following properties:

This call executes the plan by either doing the work personally
or by delegating to other dispatchers.

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} the posted body was not valid JSON, or something
went wrong with the startup.
@endDocuBlock

