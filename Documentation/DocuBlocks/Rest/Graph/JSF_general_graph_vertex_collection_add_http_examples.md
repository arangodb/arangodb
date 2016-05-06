@startDocuBlock JSF_general_graph_vertex_collection_add_http_examples
@brief Add an additional vertex collection to the graph.

@RESTHEADER{POST /_api/gharial/{graph-name}/vertex, Add vertex collection}

@RESTDESCRIPTION
Adds a vertex collection to the set of collections of the graph. If
the collection does not exist, it will be created.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the edge collection could be added successfully and
waitForSync is true.

@RESTRETURNCODE{202}
Returned if the edge collection could be added successfully and
waitForSync is false.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialAddVertexCol}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/vertex";
  body = {
    collection: "otherVertices"
  };
  var response = logCurlRequest('POST', url, body);

  assert(response.code === 202);

  logJsonResponse(response);
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
