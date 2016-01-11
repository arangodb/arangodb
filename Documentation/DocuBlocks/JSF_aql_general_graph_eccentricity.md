////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_aql_general_graph_eccentricity
///
/// `GRAPH_ECCENTRICITY (graphName, options)`
///
/// The GRAPH\_ECCENTRICITY function returns the normalized
/// [eccentricity](http://en.wikipedia.org/wiki/Distance_%28graph_theory%29)
/// of the graphs vertices
///
/// The complexity of the function is described
/// [here](#the-complexity-of-the-shortest-path-algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *options* (optional) : An object containing the following options:
///   * *direction*       : The direction of the edges as a string.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *algorithm*       : The algorithm to calculate the shortest paths as a string. Possible
/// values are [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
///  (default) and [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*         : The name of the attribute of the edges containing the length as a string.
///   * *defaultWeight*   : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the eccentricity of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEccentricity1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ECCENTRICITY('routeplanner')").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the eccentricity of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEccentricity2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ECCENTRICITY('routeplanner', {weight : 'distance'})"
///   ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////