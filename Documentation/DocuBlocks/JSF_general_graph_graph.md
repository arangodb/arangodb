////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_graph
/// @brief Get a graph
///
/// `graph_module._graph(graphName)`
///
/// A graph can be retrieved by its name.
///
/// @PARAMS
///
/// @PARAM{graphName, string, required}
/// Unique identifier of the graph
///
/// @EXAMPLES
///
/// Get a graph:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphLoadGraph}
/// ~ var examples = require("@arangodb/graph-examples/example-graph.js");
/// ~ var g1 = examples.loadGraph("social");
///   var graph_module = require("@arangodb/general-graph");
///   graph = graph_module._graph("social");
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////