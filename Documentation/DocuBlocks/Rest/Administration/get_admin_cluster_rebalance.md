@startDocuBlock get_admin_cluster_rebalance
@brief Calculates the current cluster imbalance.

@RESTHEADER{GET /_admin/cluster/rebalanc, Calculates the current cluster imbalance}

@RESTDESCRIPTION
Calculates the current cluster imbalance and returns the result.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200.

@RESTREPLYBODY{leader,object,required,}
Contains information about leader imbalance.

@RESTREPLYBODY{shard,string,required,}
Contains information about shard imbalance.

@RESTREPLYBODY{version,string,required,}
The server version as a string.

@endDocuBlock
