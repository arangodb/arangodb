
@startDocuBlock put_admin_cluster_rebalance
@brief Computes and executes a set of move shard operations to improve balance.

@RESTHEADER{PUT /_admin/cluster/rebalance, Compute and execute a set of move shard operations to improve balance}

@RESTBODYPARAM{,object,required,rebalance_compute}

@RESTDESCRIPTION
Compute a set of move shard operations to improve balance.
These moves are then immediately executed.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API returns HTTP 200.

@RESTREPLYBODY{,object,required,rebalance_moves}
Executed move shard operations.

@endDocuBlock
