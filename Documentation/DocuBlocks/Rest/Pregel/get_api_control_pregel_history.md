@startDocuBlock get_api_control_pregel_history
@brief Get the overview of all Pregel jobs

@RESTHEADER{GET /_api/control_pregel/history, Get the execution statistics of all Pregel jobs, listPregelJobsStatisics}

@RESTDESCRIPTION
Returns a list of currently running and finished Pregel jobs without retrieving
their results.

The execution statistics are persisted to a system collection and kept until you
remove them, whereas the `/_api/control_pregel` endpoint only keeps the
information temporarily in memory.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the list of jobs can be retrieved successfully.

@RESTREPLYBODY{,array,required,get_api_control_pregel_struct}
A list of objects describing the Pregel jobs.

@EXAMPLES

Get the status of all active and past Pregel jobs:

@EXAMPLE_ARANGOSH_RUN{RestPregelConnectedComponentsStatistics}

  var assertInstanceOf = require("jsunity").jsUnity.assertions.assertInstanceOf;
  var examples = require("@arangodb/graph-examples/example-graph.js");
  print("5. Creating Pregel graph");
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

  const id = internal.arango.POST(url, body);
  const historyUrl = `/_api/control_pregel/history`;

  var response = logCurlRequest("GET", historyUrl);
  assert(response.code === 200);
  assertInstanceOf(Array, response.parsedBody);
  assert(response.parsedBody.length > 0);

  internal.arango.DELETE(url + id);

  logJsonResponse(response);
  examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
