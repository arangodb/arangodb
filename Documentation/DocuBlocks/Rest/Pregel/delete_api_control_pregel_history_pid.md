@startDocuBlock delete_api_control_pregel_history_pid
@brief Removes the persisted state of a past Pregel execution job

@RESTHEADER{DELETE /_api/control_pregel/history/{id}, Remove state of past pregel execution, deletePregelJobState}

@RESTURLPARAMETERS

@RESTURLPARAM{id,number,required}
Pregel execution identifier.

@RESTDESCRIPTION
Removes the persisted state of a past pregel execution.

@RESTRETURNCODES

@RESTRETURNCODE{200}
HTTP 200 is returned if the job execution ID was valid.

@RESTRETURNCODE{404}
An HTTP 404 error is returned if no Pregel job with the specified execution number
is found or the execution number is invalid.

@EXAMPLES

Removes the persisted state of a past pregel execution:

@EXAMPLE_ARANGOSH_RUN{RestPregelCancelConnectedComponentsRemoveStatePid}

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
  var historyUrl = "/_api/control_pregel/history/" + id;

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
