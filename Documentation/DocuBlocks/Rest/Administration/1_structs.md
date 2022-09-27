

@RESTSTRUCT{name,api_task_struct,string,required,}
The fully qualified name of the user function

@RESTSTRUCT{id,api_task_struct,string,required,}
A string identifying the task

@RESTSTRUCT{created,api_task_struct,number,required,float}
The timestamp when this task was created

@RESTSTRUCT{type,api_task_struct,string,required,}
What type of task is this [ `periodic`, `timed`]
  - periodic are tasks that repeat periodically
  - timed are tasks that execute once at a specific time

@RESTSTRUCT{period,api_task_struct,number,required,}
this task should run each `period` seconds

@RESTSTRUCT{offset,api_task_struct,number,required,float}
time offset in seconds from the created timestamp

@RESTSTRUCT{command,api_task_struct,string,required,}
the javascript function for this task

@RESTSTRUCT{database,api_task_struct,string,required,}
the database this task belongs to


@RESTSTRUCT{version,rebalance_compute,number,required,}
Must be set to `1`.

@RESTSTRUCT{maximumNumberOfMoves,rebalance_compute,number,optional,}
Maximum number of moves to be computed. (Default: `1000`)

@RESTSTRUCT{leaderChanges,rebalance_compute,boolean,optional,}
Allow leader changes without moving data. (Default: `true`)

@RESTSTRUCT{moveLeaders,rebalance_compute,boolean,optional,}
Allow moving leaders. (Default: `false`)

@RESTSTRUCT{moveFollowers,rebalance_compute,boolean,optional,}
Allow moving followers. (Default: `false`)

@RESTSTRUCT{piFactor,rebalance_compute,number,optional,}
(Default: `256e6`)

@RESTSTRUCT{databasesExcluded,rebalance_compute,array,optional,string}
A list of database names to exclude from the analysis. (Default: `[]`)

@RESTSTRUCT{leader,rebalance_imbalance,object,required,leader_imbalance_struct}
Information about the leader imbalance.

@RESTSTRUCT{weightUsed,leader_imbalance_struct,array,required,integer}
The weight of leader shards per DB-Server. A leader has a weight of 1 by default
but it is higher if collections can only be moved together because of
`distributeShardsLike`.

@RESTSTRUCT{targetWeight,leader_imbalance_struct,array,required,integer}
The ideal weight of leader shards per DB-Server.

@RESTSTRUCT{numberShards,leader_imbalance_struct,array,required,integer}
The number of leader shards per DB-Server.

@RESTSTRUCT{leaderDupl,leader_imbalance_struct,array,required,integer}
The measure of the leader shard distribution. The higher the number, the worse
the distribution.

@RESTSTRUCT{totalWeight,leader_imbalance_struct,integer,required,}
The sum of all weights.

@RESTSTRUCT{imbalance,leader_imbalance_struct,integer,required,}
The measure of the total imbalance. A high value indicates a high imbalance.

@RESTSTRUCT{totalShards,leader_imbalance_struct,integer,required,}
The sum of shards, counting leader shards only.

@RESTSTRUCT{shards,rebalance_imbalance,object,required,shard_imbalance_struct}
Information about the shard imbalance.

@RESTSTRUCT{sizeUsed,shard_imbalance_struct,array,required,integer}
The size of shards per DB-Server. 

@RESTSTRUCT{targetSize,shard_imbalance_struct,array,required,integer}
The ideal size of shards per DB-Server.

@RESTSTRUCT{numberShards,shard_imbalance_struct,array,required,integer}
The number of leader and follower shards per DB-Server.

@RESTSTRUCT{totalUsed,shard_imbalance_struct,integer,required,}
The sum of the sizes.

@RESTSTRUCT{totalShards,shard_imbalance_struct,integer,required,}
The sum of shards, counting leader and follower shards.

@RESTSTRUCT{imbalance,shard_imbalance_struct,integer,required,}
The measure of the total imbalance. A high value indicates a high imbalance.

@RESTSTRUCT{from,move_shard_operation,string,required,}
The server name from which to move.

@RESTSTRUCT{to,move_shard_operation,string,required,}
The ID of the destination server.

@RESTSTRUCT{shard,move_shard_operation,string,required,}
Shard ID of the shard to be moved.

@RESTSTRUCT{collection,move_shard_operation,number,required,}
Collection ID of the collection the shard belongs to.

@RESTSTRUCT{isLeader,move_shard_operation,boolean,required,}
True if this is a leader move shard operation.

@RESTSTRUCT{code,rebalance_moves,number,required,}
The status code.

@RESTSTRUCT{error,rebalance_moves,boolean,required,}
Whether an error occurred. `false` in this case.

@RESTSTRUCT{result,rebalance_moves,object,required,rebalance_result}
The result object.

@RESTSTRUCT{imbalanceBefore,rebalance_result,object,required,rebalance_imbalance}
Imbalance before the suggested move shard operations are applied.

@RESTSTRUCT{imbalanceAfter,rebalance_result,object,required,rebalance_imbalance}
Expected imbalance after the suggested move shard operations are applied.

@RESTSTRUCT{moves,rebalance_result,array,required,move_shard_operation}
The suggested move shard operations.
