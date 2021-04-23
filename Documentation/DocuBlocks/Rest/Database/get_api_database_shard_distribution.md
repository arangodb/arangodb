
@startDocuBlock get_api_database_shard_distribution
@brief provides the shard distribution statistics of the current database in the cluster

@RESTHEADER{GET /_api/database/shardDistribution, Shard distribution statistics for the current database}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{DBserver,string,optional}
Restrict results to include data from only a single DB-Server.

@RESTDESCRIPTION
Return the number of collections, shards, leaders and followers for the
database it is run inside.

This API can only be used on Coordinators, and requires read access to the
database it is run inside.

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} the parameter DBserver is not the ID of a DB-Server.

@RESTRETURNCODE{403} server is not a Coordinator.
@endDocuBlock
