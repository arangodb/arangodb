////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_aql_general_graph_absolute_closeness
///
/// `GRAPH_ABSOLUTE_CLOSENESS (graphName, vertexExample, options)`
///
/// The GRAPH\_ABSOLUTE\_CLOSENESS function returns the
/// [closeness](http://en.wikipedia.org/wiki/Centrality#Closeness-centrality)
/// of the vertices defined by the examples.
///
/// The complexity of the function is described
/// [here](#the-complexity-of-the-shortest-path-algorithms).
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertexExample*     : An example for the desired
/// vertices (see [example](#short-explanation-of-the-example-parameter)).
/// * *options*     : An object containing the following options:
///   * *direction*                        : The direction of the edges.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *startVertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   start vertex of a path.
///   * *endVertexCollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   end vertex of a path.
///   * *edgeExamples*                     : A filter example for the
/// edges in the shortest paths (
/// see [example](#short-explanation-of-the-example-parameter)).
///   * *algorithm*                        : The algorithm to calculate
/// the shortest paths. Possible values are
/// [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) (default)
///  and [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*                           : The name of the attribute of
/// the edges containing the length.
///   * *defaultWeight*                    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute closeness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsCloseness1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ABSOLUTE_CLOSENESS('routeplanner', {})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsCloseness2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_CLOSENESS("
///   +"'routeplanner', {}, {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all German cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsCloseness3}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_CLOSENESS("
/// | + "'routeplanner', {}, {startVertexCollectionRestriction : 'germanCity', " +
///   "direction : 'outbound', weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////