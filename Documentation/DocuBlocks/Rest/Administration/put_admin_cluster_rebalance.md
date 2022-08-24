
@startDocuBlock put_admin_cluster_rebalance
@brief Compute a set of move shard operations to improve balance and immediately execute these.

@RESTHEADER{PUT /_admin/cluster/rebalance, Compute a set of move shard operations to improve balance and execute.}

@RESTBODYPARAM{version,number,required,}
Must be set to `1`.

@RESTBODYPARAM{maximumNumberOfMoves,number,optional,}
Maximum number of moves to be computed. (Default `1000`)

@RESTBODYPARAM{leaderChanges,boolean,optional,}
Allow leader changes without moving data. (Default `true`)

@RESTBODYPARAM{moveLeaders,boolean,optional,}
Allow moving leaders. (Default `false`)

@RESTBODYPARAM{moveFollowers,boolean,optional,}
Allow moving followers. (Default `false`)

@RESTBODYPARAM{piFactor,number,optional,}
(Default `256e6`)

@RESTBODYPARAM{databasesExcluded,array,optional,string}
List of database names that are excluded from the analysis. (Default [])

@RESTDESCRIPTION
Compute a set of move shard operations to improve balance. These moves are then immediately executed.

@RESTRETURNCODES

@RESTRETURNCODE{200}
This API will return HTTP 200.

@RESTREPLYBODY{moves,array,required,move_shard_operation}
Executed move shard operations.

@RESTREPLYBODY{imbalanceBefore,object,required,leader_shards_imbalance_struct}
Imbalance before the suggested move shard operations were applied.
@RESTREPLYBODY{imbalanceAfter,object,required,leader_shards_imbalance_struct}
Expected imbalance after the suggested move shard operations were applied.

@RESTREPLYBODY{shards,object,required,shard_imbalance_struct}
@RESTREPLYBODY{leader,object,required,leader_imbalance_struct}

@endDocuBlock
