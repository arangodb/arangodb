@startDocuBlock JSF_general_graph_edge_definition_remove_http_examples
@brief Remove an edge definition form the graph

@RESTHEADER{DELETE /_api/gharial/{graph-name}/edge/{definition-name}, Remove an edge definition from the graph}

@RESTDESCRIPTION
Remove one edge definition from the graph.  This will only remove the
edge collection, the vertex collections remain untouched and can still
be used in your queries.

@RESTURLPARAMETERS

@RESTPARAM{graph-name, string, required}
The name of the graph.

@RESTPARAM{definition-name, string, required}
The name of the edge collection used in the definition.

@RESTQUERYPARAMETERS

@RESTPARAM{dropCollection, boolean, optional}
Drop the collection as well.
Collection will only be dropped if it is not used in other graphs.

@RESTRETURNCODES

@RESTRETURNCODE{201}
Returned if the edge definition could be removed from the graph 
and waitForSync is true.

@RESTRETURNCODE{202}
Returned if the edge definition could be removed from the graph and
waitForSync is false.

@RESTRETURNCODE{400}
Returned if no edge definition with this name is found in the graph.

@RESTRETURNCODE{404}
Returned if no graph with this name could be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{HttpGharialEdgeDefinitionRemove}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  examples.loadGraph("social");
  var url = "/_api/gharial/social/edge/relation";
  var response = logCurlRequest('DELETE', url);

  assert(response.code === 202);

  logJsonResponse(response);
  db._drop("relation");
  examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
