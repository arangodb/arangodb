@startDocuBlock get_admin_cluster_rebalance
@brief Computes the current cluster imbalance.

@RESTHEADER{GET /_admin/cluster/rebalance, Compute the current cluster imbalance}

@RESTDESCRIPTION
Computes the current cluster imbalance and returns the result. 
It additionally shows the amount of ongoing and pending move shard operations.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API returns HTTP 200.

@RESTREPLYBODY{code,number,required,}
The status code.

@RESTREPLYBODY{error,boolean,required,}
Whether an error occurred. `false` in this case.

@RESTREPLYBODY{result,object,required,get_admin_cluster_rebalance_result}
The result object.

@RESTSTRUCT{leader,get_admin_cluster_rebalance_result,object,required,leader_imbalance_struct}
Information about the leader imbalance.

@RESTSTRUCT{shards,get_admin_cluster_rebalance_result,object,required,shard_imbalance_struct}
Information about the shard imbalance.

@RESTREPLYBODY{pendingMoveShards,number,required,}
The number of pending move shard operations.

@RESTREPLYBODY{todoMoveShards,number,required,}
The number of planned move shard operations.

@endDocuBlock
