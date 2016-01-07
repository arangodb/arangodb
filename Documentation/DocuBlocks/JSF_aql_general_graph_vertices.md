////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_aql_general_graph_vertices
/// The GRAPH\_VERTICES function returns all vertices.
///
/// `GRAPH_VERTICES (graphName, vertexExample, options)`
///
/// According to the optional filters it will only return vertices that have
/// outbound, inbound or any (default) edges.
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertexExample*      : An example for the desired
/// vertices (see [example](#short-explanation-of-the-example-parameter)).
/// * *options* (optional)           : An object containing the following options:
///   * *direction*        : The direction of the edges as a string. Possible values are
///      *outbound*, *inbound* and *any* (default).
///   * *vertexCollectionRestriction*      : One or multiple
/// vertex collections that should be considered.
///
/// @EXAMPLES
///
/// A route planner example, all vertices of the graph
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertices1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_VERTICES("
///   +"'routeplanner', {}) RETURN e").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all vertices from collection *germanCity*.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertices2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_VERTICES("
/// | +"'routeplanner', {}, {direction : 'any', vertexCollectionRestriction" +
///   " : 'germanCity'}) RETURN e").toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////