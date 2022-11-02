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
HTTP 200 is returned in case the job execution ID was valid and the state is
returned along with the response.

@RESTREPLYBODY{,object,required,get_api_control_pregel}

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
    if (status.error || ["done", "canceled", "fatal error"].includes(status.state)) {
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
  examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
