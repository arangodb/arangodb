@startDocuBlock delete_api_control_pregel_history
@brief Removes the persisted state of all past Pregel jobs

@RESTHEADER{DELETE /_api/control_pregel/history, Remove the execution statistics of all past Pregel jobs, deleteAllPregelJobStatistics}

@RESTDESCRIPTION
Removes the persisted execution statistics of all past Pregel jobs.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if all persisted execution statistics have been successfully deleted.

@EXAMPLES

Remove the persisted execution statistics of all past Pregel jobs:

@EXAMPLE_ARANGOSH_RUN{RestPregelConnectedComponentsRemoveStatistics}

  var examples = require("@arangodb/graph-examples/example-graph.js");
  print("7. Creating Pregel graph");
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
      print(`7. Waiting for Pregel job ${id} (${status.state})...`);
      internal.sleep(0.5);
    }
  }

  const deleteUrl = "/_api/control_pregel/history";
  var response = logCurlRequest("DELETE", deleteUrl);
  assert(response.code === 200);

  logJsonResponse(response);
  examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
