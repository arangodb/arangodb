
@startDocuBlock post_admin_cluster_rebalance
@brief Compute a set of move shard operations to improve balance.

@RESTHEADER{POST /_admin/cluster/rebalance, Compute a set of move shard operations to improve balance}

@RESTBODYPARAM{,object,required,rebalance_compute}

@RESTDESCRIPTION
Compute a set of move shard operations to improve balance.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API returns HTTP 200.

@RESTREPLYBODY{,object,required,rebalance_moves}

@endDocuBlock
