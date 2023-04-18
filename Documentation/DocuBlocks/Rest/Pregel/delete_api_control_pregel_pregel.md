@startDocuBlock delete_api_control_pregel_pregel
@brief Cancel an ongoing Pregel execution

@RESTHEADER{DELETE /_api/control_pregel/{id}, Cancel Pregel job execution, deletePregelJob}

@RESTURLPARAMETERS

@RESTURLPARAM{id,number,required}
Pregel execution identifier.

@RESTDESCRIPTION
Cancel an execution which is still running, and discard any intermediate
results. This immediately frees all memory taken up by the execution, and
makes you lose all intermediary data.

You might get inconsistent results if you requested to store the results and
then cancel an execution when it is already in its `"storing"` state (or
`"done"` state in versions prior to 3.7.1). The data is written multi-threaded
into all collection shards at once. This means there are multiple transactions
simultaneously. A transaction might already be committed when you cancel the
execution job. Therefore, you might see some updated documents, while other
documents have no or stale results from a previous execution.

@RESTRETURNCODES

@RESTRETURNCODE{200}
HTTP 200 is returned if the job execution ID was valid.

@RESTRETURNCODE{404}
An HTTP 404 error is returned if no Pregel job with the specified execution number
is found or the execution number is invalid.

@EXAMPLES

Cancel a Pregel job to stop the execution or to free up the results if it was
started with `"store": false` and is in the done state:

@EXAMPLE_ARANGOSH_RUN{RestPregelCancelConnectedComponents}

  var examples = require("@arangodb/graph-examples/example-graph.js");
  print("4. Creating Pregel graph");
  var graph = examples.loadGraph("connectedComponentsGraph");

  var url = "/_api/control_pregel";
  var body = {
    algorithm: "wcc",
    graphName: "connectedComponentsGraph",
    params: {
      maxGSS: graph.components.count(),
      store: false
    }
  };
  var id = internal.arango.POST(url, body);

  var statusUrl = "/_api/control_pregel/" + id;
  var response = logCurlRequest("DELETE", statusUrl);

  assert(response.code === 200);

  logJsonResponse(response);
  examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
