
@startDocuBlock post_admin_cluster_rebalance_execute
@brief Executes the given set of move shard operations.

@RESTHEADER{POST /_admin/cluster/rebalance/execute, Execute a set of move shard operations}

@RESTDESCRIPTION
Execute the given set of move shard operations. You can use the
`POST /_admin/cluster/rebalance` endpoint to calculate these operations to improve
the balance of shards, leader shards, and follower shards.

@RESTBODYPARAM{version,number,required,}
Must be set to `1`.

@RESTBODYPARAM{moves,array,required,move_shard_operation}
A set of move shard operations to execute.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API returns HTTP 200 if no operations are provided.

@RESTRETURNCODE{202}
This API returns HTTP 202 if the operations have been accepted and scheduled for execution.

@endDocuBlock
