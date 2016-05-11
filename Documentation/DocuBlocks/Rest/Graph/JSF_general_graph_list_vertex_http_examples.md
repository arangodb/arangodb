
@startDocuBlock JSF_general_graph_list_vertex_http_examples
@brief Lists all vertex collections used in this graph.

@RESTHEADER{GET /_api/gharial/{graph-name}/vertex, List vertex collections}

@RESTDESCRIPTION
Lists all vertex collections within this graph.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the collections could be listed.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialListVertex}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/vertex";
  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

