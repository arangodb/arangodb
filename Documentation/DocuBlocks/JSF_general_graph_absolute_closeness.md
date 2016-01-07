////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_absolute_closeness
/// @brief Get the
/// [closeness](http://en.wikipedia.org/wiki/Centrality#Closeness_centrality)
/// of the vertices defined by the examples.
///
/// `graph._absoluteCloseness(vertexExample, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for *vertexExample*.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.md#the-complexity-of-the-shortest-path-algorithms).
///
/// @PARAMS
///
/// @PARAM{vertexExample, object, optional}
/// Filter the vertices, see [Definition of examples](#definition-of-examples)
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *startVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for source vertices.
///   * *endVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for target vertices.
///   * *edgeExamples*: Filter the edges to be followed, see [Definition of examples](#definition-of-examples)
///   * *algorithm*: The algorithm to calculate the shortest paths, possible values are
///       [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
///       [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the closeness can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute closeness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsCloseness1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteCloseness({});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsCloseness2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteCloseness({}, {weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all German Cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsCloseness3}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | graph._absoluteCloseness({}, {startVertexCollectionRestriction : 'germanCity',
///   direction : 'outbound', weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////