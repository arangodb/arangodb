
@startDocuBlock put_admin_cluster_rebalance
@brief Computes and executes a set of move shard operations to improve balance.

@RESTHEADER{PUT /_admin/cluster/rebalance, Compute and execute a set of move shard operations to improve balance, startClusterRebalance}

@RESTBODYPARAM{,object,required,rebalance_compute}
The options for the rebalancing.

@RESTDESCRIPTION
Compute a set of move shard operations to improve balance.
These moves are then immediately executed.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API returns HTTP 200.

@RESTREPLYBODY{,object,required,rebalance_moves}
The executed move shard operations.

@endDocuBlock
