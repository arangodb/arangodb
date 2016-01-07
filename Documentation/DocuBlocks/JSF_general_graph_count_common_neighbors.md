////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_count_common_neighbors
/// @brief Get the amount of common neighbors of the vertices defined by the examples.
///
/// `graph._countCommonNeighbors(vertex1Example, vertex2Examples, optionsVertex1, optionsVertex2)`
///
/// Similar to [_commonNeighbors](#commonneighbors) but returns count instead of the elements.
///
/// @EXAMPLES
///
/// A route planner example, all common neighbors of capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighborsAmount1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   var example = { isCapital: true };
///   var options = { includeData: true };
///   graph._countCommonNeighbors(example, example, options, options);
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all common outbound neighbors of Hamburg with any other location
/// which have a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighborsAmount2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   var options = { direction: 'outbound', maxDepth: 2, includeData: true };
///   graph._countCommonNeighbors('germanCity/Hamburg', {}, options, options);
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////