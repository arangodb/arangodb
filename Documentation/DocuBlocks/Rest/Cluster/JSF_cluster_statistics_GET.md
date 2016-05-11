
@startDocuBlock JSF_cluster_statistics_GET
@brief allows to query the statistics of a DBserver in the cluster

@RESTHEADER{GET /_admin/clusterStatistics, Queries statistics of DBserver}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{DBserver,string,required}

@RESTDESCRIPTION Queries the statistics of the given DBserver

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} the parameter DBserver was not given or is not the
ID of a DBserver

@RESTRETURNCODE{403} server is not a coordinator.
@endDocuBlock

