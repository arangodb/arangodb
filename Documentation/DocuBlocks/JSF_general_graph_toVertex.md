////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_toVertex
/// @brief Get the target vertex of an edge
///
/// `graph._toVertex(edgeId)`
///
/// Returns the vertex defined with the attribute *_to* of the edge with *edgeId* as its *_id*.
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphGetToVertex}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._toVertex("relation/aliceAndBob")
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////