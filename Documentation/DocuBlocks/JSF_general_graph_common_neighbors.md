////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_common_neighbors
/// @brief Get all common neighbors of the vertices defined by the examples.
///
/// `graph._commonNeighbors(vertex1Example, vertex2Examples, optionsVertex1, optionsVertex2)`
///
/// This function returns the intersection of *graph_module._neighbors(vertex1Example, optionsVertex1)*
/// and *graph_module._neighbors(vertex2Example, optionsVertex2)*.
/// For parameter documentation see [_neighbors](#neighbors).
///
/// The complexity of this method is **O(n\*m^x)** with *n* being the maximal amount of vertices
/// defined by the parameters vertexExamples, *m* the average amount of neighbors and *x* the
/// maximal depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// @EXAMPLES
///
/// A route planner example, all common neighbors of capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighbors1}
/// var examples = require("@arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._commonNeighbors({isCapital : true}, {isCapital : true});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all common outbound neighbors of Hamburg with any other location
/// which have a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighbors2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | graph._commonNeighbors(
/// |   'germanCity/Hamburg',
/// |   {},
/// |   {direction : 'outbound', maxDepth : 2},
///     {direction : 'outbound', maxDepth : 2});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////