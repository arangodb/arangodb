@startDocuBlock delete_api_control_pregel_history_id
@brief Removes the persisted state of a past Pregel execution job

@RESTHEADER{DELETE /_api/control_pregel/history/{id}, Remove the execution statistics of a past Pregel job, deletePregelJobStatistics}

@RESTURLPARAMETERS

@RESTURLPARAM{id,number,required}
The Pregel job identifier.

@RESTDESCRIPTION
Removes the persisted execution statistics of a finished Pregel job.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the Pregel job ID is valid.

@RESTRETURNCODE{404}
is returned if no Pregel job with the specified ID is found or if the ID
is invalid.

@EXAMPLES

Remove the persisted execution statistics of a finished Pregel job:

@EXAMPLE_ARANGOSH_RUN{RestPregelConnectedComponentsRemoveStatisticsId}

  var examples = require("@arangodb/graph-examples/example-graph.js");
  print("8. Creating graph");
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

  const statusUrl = `${url}/${id}`;
  while (true) {
    var status = internal.arango.GET(statusUrl);
    if (status.error || ["done", "canceled", "fatal error"].includes(status.state)) {
      assert(status.state == "done");
      break;
    } else {
      print(`8. Waiting for Pregel job ${id} (${status.state})...`);
      internal.sleep(0.5);
    }
  }

  const historyUrl = `/_api/control_pregel/history/${id}`
  var response = logCurlRequest("DELETE", historyUrl);
  assert(response.code === 200);

  logJsonResponse(response);
  examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
