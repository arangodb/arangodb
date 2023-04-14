@startDocuBlock delete_api_control_pregel_history
@brief Removes the persisted state of all past Pregel jobs

@RESTHEADER{DELETE /_api/control_pregel/history, Remove the persisted state of all past Pregel execution jobs, deletePregelAllJobsState}

@RESTDESCRIPTION
Removes the persisted state of a all past Pregel job executions.

@RESTRETURNCODES

@RESTRETURNCODE{200}
HTTP 200 is returned if all persisted job states could be successfully deleted.

@EXAMPLES

Removes the persisted states of all past Pregel executions:

@EXAMPLE_ARANGOSH_RUN{RestPregelCancelConnectedComponentsRemoveState}

  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("connectedComponentsGraph");

  var postUrl = "/_api/control_pregel";
  var body = {
    algorithm: "wcc",
    graphName: "connectedComponentsGraph",
    params: {
      maxGSS: graph.components.count(),
      store: false
    }
  };
  var id = internal.arango.POST(postUrl, body);
  var historyUrl = "/_api/control_pregel/history;

  while (true) {
    var status = internal.arango.GET(url);
    if (status.error || ["done", "canceled", "fatal error"].includes(status.state)) {
      assert(status.state == "done");
      break;
    } else {
      print(`Waiting for Pregel job ${id} (${status.state})...`);
      internal.sleep(0.5);
    }
  }

  var response = logCurlRequest("DELETE", historyUrl);
  assert(response.code === 200);

  logJsonResponse(response);
  examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
