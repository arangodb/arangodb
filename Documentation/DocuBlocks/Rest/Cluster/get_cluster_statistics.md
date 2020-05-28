
@startDocuBlock get_cluster_statistics
@brief allows to query the statistics of a DB-Server in the cluster

@RESTHEADER{GET /_admin/clusterStatistics, Queries statistics of a DB-Server}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{DBserver,string,required}

@RESTDESCRIPTION
Queries the statistics of the given DB-Server

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} the parameter DBserver was not given or is not the
ID of a DB-Server

@RESTRETURNCODE{403} server is not a Coordinator.
@endDocuBlock
