@RESTSTRUCT{id,get_api_control_pregel,string,required,string}
The ID of the Pregel job, as a string.

@RESTSTRUCT{algorithm,get_api_control_pregel,string,required,string}
The algorithm used by the job.

@RESTSTRUCT{created,get_api_control_pregel,string,required,string}
The date and time when the job was created.

@RESTSTRUCT{expires,get_api_control_pregel,string,optional,string}
The date and time when the job results expire. The expiration date is only
meaningful for jobs that were completed, canceled or resulted in an error. Such jobs
are cleaned up by the garbage collection when they reach their expiration date/time.

@RESTSTRUCT{ttl,get_api_control_pregel,number,required,float}
The TTL (time to live) value for the job results, specified in seconds.
The TTL is used to calculate the expiration date for the job's results.

@RESTSTRUCT{state,get_api_control_pregel,string,required,string}
The state of the execution. The following values can be returned:
- `"none"`: The Pregel run did not yet start.
- `"loading"`: The graph is loaded from the database into memory before the execution of the algorithm.
- `"running"`: The algorithm is executing normally.
- `"storing"`: The algorithm finished, but the results are still being written
  back into the collections. Occurs only if the store parameter is set to true.
- `"done"`: The execution is done. In version 3.7.1 and later, this means that
  storing is also done. In earlier versions, the results may not be written back
  into the collections yet. This event is announced in the server log (requires
  at least info log level for the `pregel` log topic).
- `"canceled"`: The execution was permanently canceled, either by the user or by
  an error.
- `"fatal error"`: The execution has failed and cannot recover.
- `"in error"`: The execution is in an error state. This can be
  caused by DB-Servers being not reachable or being non responsive. The execution
  might recover later, or switch to `"canceled"` if it was not able to recover
  successfully. 
- `"recovering"` (currently unused): The execution is actively recovering and
  switches back to `running` if the recovery is successful.

@RESTSTRUCT{gss,get_api_control_pregel,integer,required,int64}
The number of global supersteps executed.

@RESTSTRUCT{totalRuntime,get_api_control_pregel,number,required,float}
The total runtime of the execution up to now (if the execution is still ongoing).

@RESTSTRUCT{startupTime,get_api_control_pregel,number,required,float}
The startup runtime of the execution.
The startup time includes the data loading time and can be substantial.

@RESTSTRUCT{computationTime,get_api_control_pregel,number,required,float}
The algorithm execution time. Is shown when the computation started. 

@RESTSTRUCT{storageTime,get_api_control_pregel,number,optional,float}
The time for storing the results if the job includes results storage.
Is shown when the storing started.

@RESTSTRUCT{gssTimes,get_api_control_pregel,array,optional,number}
Computation time of each global super step. Is shown when the computation started.

@RESTSTRUCT{reports,get_api_control_pregel,array,required,object}
This attribute is used by Programmable Pregel Algorithms (`air`, experimental).
The value is only populated once the algorithm has finished.

@RESTSTRUCT{vertexCount,get_api_control_pregel,integer,optional,int64}
The total number of vertices processed.

@RESTSTRUCT{edgeCount,get_api_control_pregel,integer,optional,int64}
The total number of edges processed.

@RESTSTRUCT{detail,get_api_control_pregel,object,required,get_api_control_pregel_detail}
The Pregel run details.

@RESTSTRUCT{aggregatedStatus,get_api_control_pregel_detail,object,required,get_api_control_pregel_detail_aggregated}
The aggregated details of the full Pregel run. The values are totals of all the
DB-Server.

@RESTSTRUCT{timeStamp,get_api_control_pregel_detail_aggregated,string,required,date-time}
The time at which the status was measured.

@RESTSTRUCT{graphStoreStatus,get_api_control_pregel_detail_aggregated,object,optional,get_api_control_pregel_detail_aggregated_store}
The status of the in memory graph.

@RESTSTRUCT{verticesLoaded,get_api_control_pregel_detail_aggregated_store,integer,optional,int64}
The number of vertices that are loaded from the database into memory.

@RESTSTRUCT{edgesLoaded,get_api_control_pregel_detail_aggregated_store,integer,optional,int64}
The number of edges that are loaded from the database into memory.

@RESTSTRUCT{memoryBytesUsed,get_api_control_pregel_detail_aggregated_store,integer,optional,int64}
The number of bytes used in-memory for the loaded graph.

@RESTSTRUCT{verticesStored,get_api_control_pregel_detail_aggregated_store,integer,optional,int64}
The number of vertices that are written back to the database after the Pregel
computation finished. It is only set if the `store` parameter is set to `true`.

@RESTSTRUCT{allGssStatus,get_api_control_pregel_detail_aggregated,object,optional,get_api_control_pregel_detail_aggregated_gss}
Information about the global supersteps.

@RESTSTRUCT{items,get_api_control_pregel_detail_aggregated_gss,array,optional,get_api_control_pregel_detail_aggregated_gss_items}
A list of objects with details for each global superstep.

@RESTSTRUCT{verticesProcessed,get_api_control_pregel_detail_aggregated_gss_items,integer,optional,int64}
The number of vertices that have been processed in this step.

@RESTSTRUCT{messagesSent,get_api_control_pregel_detail_aggregated_gss_items,integer,optional,int64}
The number of messages sent in this step.

@RESTSTRUCT{messagesReceived,get_api_control_pregel_detail_aggregated_gss_items,integer,optional,int64}
The number of messages received in this step.

@RESTSTRUCT{memoryBytesUsedForMessages,get_api_control_pregel_detail_aggregated_gss_items,integer,optional,int64}
The number of bytes used in memory for the messages in this step.

@RESTSTRUCT{workerStatus,get_api_control_pregel_detail,object,required,}
The details of the Pregel for every DB-Server. Each object key is a DB-Server ID,
and each value is a nested object similar to the `aggregatedStatus` attribute.
In a single server deployment, there is only a single entry with an empty string as key.
