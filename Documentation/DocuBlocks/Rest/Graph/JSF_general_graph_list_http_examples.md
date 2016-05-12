
@startDocuBlock JSF_general_graph_list_http_examples
@brief Lists all graphs known to the graph module.

@RESTHEADER{GET /_api/gharial, List all graphs}

@RESTDESCRIPTION
Lists all graph names stored in this database.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the module is available and the graphs could be listed.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialList}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  examples.loadGraph("social");
  examples.loadGraph("routeplanner");
  var url = "/_api/gharial";
  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);
~ examples.dropGraph("social");
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

