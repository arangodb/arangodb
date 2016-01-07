////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_aql_general_graph_absolute_betweenness
///
/// `GRAPH_ABSOLUTE_BETWEENNESS (graphName, options)`
///
/// The GRAPH\_ABSOLUTE\_BETWEENNESS function returns the
/// [betweenness](http://en.wikipedia.org/wiki/Betweenness_centrality)
/// of all vertices in the graph.
///
/// The complexity of the function is described
/// [here](#the-complexity-of-the-shortest-path-algorithms).
///
///
/// * *graphName*          : The name of the graph as a string.
/// * *options*            : An object containing the following options:
///   * *direction*                        : The direction of the edges.
/// Possible values are *outbound*, *inbound* and *any* (default).
///   * *weight*                           : The name of the attribute of
/// the edges containing the length.
///   * *defaultWeight*                    : Only used with the option *weight*.
/// If an edge does not have the attribute named as defined in option *weight* this default
/// is used as length.
/// If no default is supplied the default would be positive Infinity so the path and
/// hence the betweenness can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute betweenness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsBetweenness1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
///   db._query("RETURN GRAPH_ABSOLUTE_BETWEENNESS('routeplanner', {})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute betweenness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsBetweenness2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_BETWEENNESS("
///   +"'routeplanner', {weight : 'distance'})").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphAbsBetweenness3}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_BETWEENNESS("
/// | + "'routeplanner', {direction : 'outbound', weight : 'distance'})"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////