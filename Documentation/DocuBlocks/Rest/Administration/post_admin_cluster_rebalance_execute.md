
@startDocuBlock post_admin_cluster_rebalance_execute
@brief Execute the given set of move shard operations.

@RESTHEADER{POST /_admin/cluster/rebalance/execute, Execute the given set of move shard operations}

@RESTDESCRIPTION
Execute the given set of move shard operations. Such a set can be calculated using
other rebalance apis.

@RESTBODYPARAM{version,number,required,}
Must be set to `1`.
@RESTBODYPARAM{moves,array,required,move_shard_operation}
Set of move shard operations to be executed.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200 if no operations were provided.
@RESTRETURNCODE{202}
This API will return HTTP 202 if the operations have been accepted and scheduled for execution.
@endDocuBlock
