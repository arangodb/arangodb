
@startDocuBlock JSF_general_graph_get_http_examples
@brief Get a graph from the graph module.

@RESTHEADER{GET /_api/gharial/{graph-name}, Get a graph}

@RESTDESCRIPTION
Gets a graph from the collection *_graphs*.
Returns the definition content of this graph.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Returned if the graph could be found.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialGetGraph}
  var graph = require("@arangodb/general-graph");
| if (graph._exists("myGraph")) {
|    graph._drop("myGraph", true);
  }
  graph._create("myGraph", [{
    collection: "edges",
    from: [ "startVertices" ],
    to: [ "endVertices" ]
  }]);
  var url = "/_api/gharial/myGraph";

  var response = logCurlRequest('GET', url);

  assert(response.code === 200);

  logJsonResponse(response);

  graph._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

