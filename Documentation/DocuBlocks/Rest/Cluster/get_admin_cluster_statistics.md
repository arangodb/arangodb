
@startDocuBlock get_admin_cluster_statistics

@RESTHEADER{GET /_admin/cluster/statistics, Get the statistics of a DB-Server, getClusterStatistics}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{DBserver,string,required}
The ID of a DB-Server.

@RESTDESCRIPTION
Queries the statistics of the given DB-Server

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned when everything went well.

@RESTRETURNCODE{400}
The `DBserver` parameter was not specified or is not the ID of a DB-Server.

@RESTRETURNCODE{403}
The specified server is not a DB-Server.

@endDocuBlock
