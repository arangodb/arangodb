
@startDocuBlock JSF_general_graph_list_edge_http_examples
@brief Lists all edge definitions

@RESTHEADER{GET /_api/gharial/{graph-name}/edge, List edge definitions}

@RESTDESCRIPTION
Lists all edge collections within this graph.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the edge definitions could be listed.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialListEdge}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/edge";
  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

