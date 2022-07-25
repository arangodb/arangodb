@startDocuBlock get_api_control_pregel_status
@brief Get the status of a Pregel execution

@RESTHEADER{GET /_api/control_pregel/{id}, Get Pregel job execution status}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{id,number,required}
Pregel execution identifier.

@RESTDESCRIPTION
Returns the current state of the execution, the current global superstep, the
runtime, the global aggregator values as well as the number of sent and
received messages.

@RESTRETURNCODES

@RESTRETURNCODE{200}
HTTP 200 will be returned in case the job execution id was valid and the state is
returned along with the response.

@RESTREPLYBODY{id,string,required,string}
An id of the Pregel job, as a string.

@RESTREPLYBODY{algorithm,string,required,string}
An algorithm used by the job.

@RESTREPLYBODY{created,string,required,string}
A date and time when the job was created.

@RESTREPLYBODY{expires,string,optional,string}
A date and time when the job results expire. The expiration date is only
meaningful for jobs that were completed, canceled or resulted in an error. Such jobs
are cleaned up by the garbage collection when they reach their expiration date/time.

@RESTREPLYBODY{ttl,number,required,float}
A TTL (time to live) value for the job results, specified in seconds.
The TTL is used to calculate the expiration date for the job's results.

@RESTREPLYBODY{state,string,required,string}
State of the execution. The following values can be returned:
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
- `"recovering"` (currently unused): The execution is actively recovering, will
  switch back to `running` if the recovery was successful.

@RESTREPLYBODY{gss,number,required,int64}
The number of global supersteps executed.

@RESTREPLYBODY{totalRuntime,number,required,float}
Total runtime of the execution up to now (if the execution is still ongoing).

@RESTREPLYBODY{startupTime,number,required,float}
Startup runtime of the execution.
The startup time includes the data loading time and can be substantial.

@RESTREPLYBODY{computationTime,number,required,float}
Algorithm execution time. Is shown when the computation started. 

@RESTREPLYBODY{storageTime,number,optional,float}
Time for storing the results if the job includes results storage. Is shown when the storing started.

@RESTREPLYBODY{gssTimes,array,optional,float}
Computation time of each global super step. Is shown when the computation started.

@RESTREPLYBODY{reports,object,optional,get_api_control_pregel_reports}
Statistics about the Pregel execution. The value will only be populated once
the algorithm has finished.

@RESTSTRUCT{vertexCount,get_api_control_pregel_reports,integer,optional,int64}
Total number of vertices processed.

@RESTSTRUCT{edgeCount,get_api_control_pregel_reports,integer,optional,int64}
Total number of edges processed.

@RESTREPLYBODY{detail,object,required,get_api_contol_pregel_detail}
Pregel run details. These details consist of the total `aggregatedStatus` and the `workerStatus`. The `workerStatus` shows the details for each DB Server individually, while the `aggregatedStatus` is an aggregate of all these DB Server details and therefore gives the details for the full Pregel run. 

A status consists of a timeStamp, graphStoreStatus and allGssStatus has the following properties:

@RESTSTRUCT{timeStamp,get_api_control_pregel_detail,time,required,time}
The time at which the status was measured.

@RESTREPLYBODY{graphStoreStatus,object,optional,get_api_control_pregel_detail_graphstorestatus}
Status of the in memory graph.

@RESTSTRUCT{verticesLoaded,get_api_control_pregel_detail_graphstorestatus,number,optional,int64}
Number of vertices that are loaded from the database into memory.

@RESTSTRUCT{edgesLoaded,get_api_control_pregel_detail_graphstorestatus,number,optional,int64}
Number of edges that are loaded from the database into memory.

@RESTSTRUCT{memoryBytesUsed,get_api_control_pregel_detail_graphstorestatus,number,optional,int64}
Number of bytes used in-memory for the loaded graph.

@RESTSTRUCT{verticesStored,get_api_control_pregel_detail_graphstorestatus,number,optional,int64}
Number of vertices that are written back to the database after the Pregel computation finished. Only occurs if the store parameter is set to true.

@RESTREPLYBODY{allGssStatus,object,optional,get_api_control_pregel_detail_allgssstatus}
Details for each global superstep, `item` includes an array with

@RESTSTRUCT{verticesProcessed,get_api_control_pregel_detail_allgssstatus,number,optional,int64}
Number of vertices that have been processed in this step.

@RESTSTRUCT{messagesSent,get_api_control_pregel_detail_allgssstatus,number,optional,int64}
Number of messages sent in this step.

@RESTSTRUCT{messagesReceived,get_api_control_pregel_detail_allgssstatus,number,optional,int64}
Number of messages received in this step.

@RESTSTRUCT{memoryBytesUsedForMessages,get_api_control_pregel_detail_allgssstatus,number,optional,int64}
Number of bytes used in memory for the messages in this step.

@RESTRETURNCODE{404}
An HTTP 404 error is returned if no Pregel job with the specified execution number
is found or the execution number is invalid.

@EXAMPLES

Get the execution status of a Pregel job:

@EXAMPLE_ARANGOSH_RUN{RestPregelStatusConnectedComponents}

  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("connectedComponentsGraph");

  var url = "/_api/control_pregel";
  var body = {
    algorithm: "wcc",
    graphName: "connectedComponentsGraph",
    params: {
      maxGSS: graph.components.count(),
      resultField: "component"
    }
  };
  var id = internal.arango.POST(url, body);
  var url = "/_api/control_pregel/" + id;
  while (true) {
    var status = internal.arango.GET(url);
    if (["done", "canceled", "fatal error"].includes(status.state)) {
      assert(status.state == "done");
      break;
    } else {
      print(`Waiting for Pregel job ${id} (${status.state})...`);
      internal.sleep(0.5);
    }
  }

  var response = logCurlRequest("GET", url);
  assert(response.code === 200);

  logJsonResponse(response);
~ examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
