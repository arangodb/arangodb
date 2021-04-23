
@startDocuBlock get_cluster_shard_distribution
@brief allows to query the global shard distribution statistics in the cluster

@RESTHEADER{GET /_admin/cluster/shardDistribution, Global statistics on the current shard distribution}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{DBserver,string,optional}
Restrict results to include data from only a single DB-Server. The response
will not contain aggregates, but instead one entry per available database will
be returned, so that the statistics are split across all available databases.

@RESTQUERYPARAM{details,boolean,optional}
Shard statistics for each database separately if set to `false` (default),
or totals if set to `true`.

@RESTDESCRIPTION
Returns global statistics on the current shard distribution, providing the
total number of databases, collections, shards, leader and follower shards
for the entire cluster.

This API can only be used in the `_system` database on Coordinators and
requires admin user privileges.

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} the parameter DBserver is not the ID of a DB-Server.

@RESTRETURNCODE{403} server is not a Coordinator.
@endDocuBlock
