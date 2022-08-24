@startDocuBlock get_admin_cluster_rebalance
@brief Calculates the current cluster imbalance.

@RESTHEADER{GET /_admin/cluster/rebalance, Calculates the current cluster imbalance}

@RESTDESCRIPTION
Calculates the current cluster imbalance and returns the result. 
It additionally shows the amount of ongoing and pending move shard operations.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200.

@RESTREPLYBODY{leader,object,required,leader_imbalance_struct}
Contains information about leader imbalance.

@RESTREPLYBODY{shard,object,required,shard_imbalance_struct}
Contains information about shard imbalance.

@RESTREPLYBODY{pendingMoveShards,number,required,}
Number of pending move shard operations.

@RESTREPLYBODY{todoMoveShards,number,required,}
Number of planned move shard operations.

@RESTSTRUCT{imbalance,leader_imbalance_struct,number,required,}
Measures the total imbalance. A high value indicates a high imbalance.

@RESTSTRUCT{imbalance,shard_imbalance_struct,number,required,}
Measures the total imbalance. A high value indicates a high imbalance.

@RESTSTRUCT{totalShards,shard_imbalance_struct,number,required,}
Total number of shards. (counting leader and follower shards)

@endDocuBlock
