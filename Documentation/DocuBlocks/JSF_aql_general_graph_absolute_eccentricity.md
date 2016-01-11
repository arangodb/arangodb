////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_aql_general_graph_absolute_eccentricity
///
/// `GRAPH_ABSOLUTE_ECCENTRICITY (graphName, vertexExample, options)`
///
///  The GRAPH\_ABSOLUTE\_ECCENTRICITY function returns the
/// [eccentricity](http://en.wikipedia.org/wiki/Distance_%28graph_theory%29)
/// of the vertices defined by the examples.
///
/// The complexity of the function is described
/// [here](#the-complexity-of-the-shortest-path-algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertexExample*      : An example for the desired
/// vertices (see [example](#short-explanation-of-the-example-parameter)).
/// * *options* (optional)    : An object containing the following options:
///   * *direction*                        : The direction of the edges as a string.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *startVertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   start vertex of a path.
///   * *endVertexCollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   end vertex of a path.
///   * *edgeExamples*                     : A filter example for the edges in the
///  shortest paths (see [example](#short-explanation-of-the-example-parameter)).
///   * *algorithm*                        : The algorithm to calculate
///  the shortest paths as a string. If vertex example is empty
///  [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) is
///  used as default, otherwise the default is
///  [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)
///   * *weight*                           : The name of the attribute of
/// the edges containing the length as a string.
///   * *defaultWeight*                    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute eccentricity of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsEccentricity1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY('routeplanner', {})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsEccentricity2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY("
///   +"'routeplanner', {}, {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all German cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsEccentricity3}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY("
/// | + "'routeplanner', {}, {startVertexCollectionRestriction : 'germanCity', " +
///   "direction : 'outbound', weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////