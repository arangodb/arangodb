@startDocuBlock get_api_control_pregel_overview
@brief Get the overview of currently running Pregel jobs

@RESTHEADER{GET /_api/control_pregel, Get currently running Pregel jobs}

@RESTDESCRIPTION
Returns a list of currently running and recently finished Pregel jobs without
retrieving their results.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned when the list of jobs can be retrieved successfully.

@RESTREPLYBODY{,array,required,get_api_control_pregel}
A list of objects describing the Pregel jobs.

@EXAMPLES

Get the status of all active Pregel jobs:

@EXAMPLE_ARANGOSH_RUN{RestPregelStatusAllConnectedComponents}

  var assertInstanceOf = require("jsunity").jsUnity.assertions.assertInstanceOf;
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

  var url = "/_api/control_pregel/";

  var response = logCurlRequest("GET", url);
  assert(response.code === 200);
  assertInstanceOf(Array, response.parsedBody);
  assert(response.parsedBody.length > 0);

  internal.arango.DELETE(url + id);

  logJsonResponse(response);
  examples.dropGraph("connectedComponentsGraph");

@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
