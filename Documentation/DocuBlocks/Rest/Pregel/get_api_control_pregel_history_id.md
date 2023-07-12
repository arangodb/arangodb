@startDocuBlock get_api_control_pregel_history_id

@RESTHEADER{GET /_api/control_pregel/history/{id}, Get the execution statistics of a Pregel job, getPregelJobStatistics}

@RESTURLPARAMETERS

@RESTURLPARAM{id,number,required}
Pregel job identifier.

@RESTDESCRIPTION
Returns the current state of the execution, the current global superstep, the
runtime, the global aggregator values, as well as the number of sent and
received messages.

The execution statistics are persisted to a system collection and kept until you
remove them, whereas the `/_api/control_pregel/{id}` endpoint only keeps the
information temporarily in memory.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the Pregel job ID is valid and the execution statistics are
returned along with the response.

@RESTREPLYBODY{,object,required,get_api_control_pregel_struct}
The information about the Pregel job.

@RESTRETURNCODE{404}
is returned if no Pregel job with the specified ID is found or if the ID
is invalid.

@EXAMPLES

Get the execution status of a Pregel job:

@EXAMPLE_ARANGOSH_RUN{RestPregelConnectedComponentsStatisticsId}

  var examples = require("@arangodb/graph-examples/example-graph.js");
  print("6. Creating Pregel graph");
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

  const statusUrl = `${url}/${id}`;
  while (true) {
    var status = internal.arango.GET(statusUrl);
    if (status.error || ["done", "canceled", "fatal error"].includes(status.state)) {
      assert(status.state == "done");
      break;
    } else {
      print(`6. Waiting for Pregel job ${id} (${status.state})...`);
      internal.sleep(0.5);
    }
  }

  const historyUrl = `/_api/control_pregel/history/${id}`;
  var response = logCurlRequest("GET", historyUrl);
  assert(response.code === 200);

  logJsonResponse(response);
  examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
