

@brief Get the source vertex of an edge

`graph._fromVertex(edgeId)`

Returns the vertex defined with the attribute *_from* of the edge with *edgeId* as its *_id*.

@PARAMS

@PARAM{edgeId, string, required}
*_id* attribute of the edge

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphGetFromVertex}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  graph._fromVertex("relation/aliceAndBob")
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT


