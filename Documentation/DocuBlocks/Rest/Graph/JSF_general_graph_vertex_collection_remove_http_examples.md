@startDocuBlock JSF_general_graph_vertex_collection_remove_http_examples
@brief Remove a vertex collection form the graph.

@RESTHEADER{DELETE /_api/gharial/{graph-name}/vertex/{collection-name}, Remove vertex collection}

@RESTDESCRIPTION
Removes a vertex collection from the graph and optionally deletes the collection,
if it is not used in any other graph.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTPARAM{collection-name, string, required}
The name of the vertex collection.

@RESTQUERYPARAMETERS

@RESTPARAM{dropCollection, boolean, optional}
Drop the collection as well.
Collection will only be dropped if it is not used in other graphs.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the vertex collection was removed from the graph successfully
and waitForSync is true.

@RESTRETURNCODE{202}
Returned if the request was successful but waitForSync is false.

@RESTRETURNCODE{400}
Returned if the vertex collection is still used in an edge definition.
In this case it cannot be removed from the graph yet, it has to be
removed from the edge definition first.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@EXAMPLES

You can remove vertex collections that are not used in any edge collection:

@EXAMPLE_ARANGOSH_RUN{HttpGharialRemoveVertexCollection}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("social");
  g._addVertexCollection("otherVertices");
  var url = "/_api/gharial/social/vertex/otherVertices";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 202);

  logJsonResponse(response);
~ examples.dropGraph("social");
~ db._drop("otherVertices");
@END_EXAMPLE_ARANGOSH_RUN

You cannot remove vertex collections that are used in edge collections:

@EXAMPLE_ARANGOSH_RUN{HttpGharialRemoveVertexCollectionFailed}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("social");
  var url = "/_api/gharial/social/vertex/male";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 400);

  logJsonResponse(response);
  db._drop("male");
  db._drop("female");
  db._drop("relation");
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
