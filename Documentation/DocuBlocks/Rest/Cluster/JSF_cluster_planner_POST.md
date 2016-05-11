
@startDocuBlock JSF_cluster_planner_POST
@brief exposes the cluster planning functionality

@RESTHEADER{POST /_admin/clusterPlanner, Produce cluster startup plan}

@RESTALLBODYPARAM{clusterPlan,object,required}
A cluster plan object

@RESTDESCRIPTION Given a description of a cluster, this plans the details
of a cluster and returns a JSON description of a plan to start up this
cluster.

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} the posted body was not valid JSON.
@endDocuBlock

