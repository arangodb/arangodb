////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_aql_general_graph_edges
///
/// `GRAPH_EDGES (graphName, vertexExample, options)`
///
/// The GRAPH\_EDGES function returns all edges of the graph connected to the vertices
/// defined by the example.
///
/// The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
/// parameter vertexExamplex, *m* the average amount of edges of a vertex and *x* the maximal
/// depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertexExample*      : An example for the desired
/// vertices (see [example](#short-explanation-of-the-example-parameter)).
/// * *options* (optional) : An object containing the following options:
///   * *direction*                        : The direction
/// of the edges as a string. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *startVertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   start vertex of a path.
///   * *endVertexCollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   end vertex of a path.
///   * *edgeExamples*                     : A filter example
/// for the edges (see [example](#short-explanation-of-the-example-parameter)).
///   * *minDepth*                         : Defines the minimal length of a path from an edge
///  to a vertex (default is 1, which means only the edges directly connected to a vertex would
///  be returned).
///   * *maxDepth*                         : Defines the maximal length of a path from an edge
///  to a vertex (default is 1, which means only the edges directly connected to a vertex would
///  be returned).
///   * *maxIterations*: the maximum number of iterations that the traversal is
///      allowed to perform. It is sensible to set this number so unbounded traversals
///      will terminate.
///   * *includeData*: Defines if the result should contain only ids (false) or if all documents
///     should be fully extracted (true). By default this parameter is set to false, so only ids
///     will be returned.
///
/// @EXAMPLES
///
/// A route planner example, all edges to locations with a distance of either 700 or 600.:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdges1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_EDGES("
/// | +"'routeplanner', {}, {edgeExamples : [{distance: 600}, {distance: 700}]}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all outbound edges of Hamburg with a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdges2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_EDGES("
/// | +"'routeplanner', 'germanCity/Hamburg', {direction : 'outbound', maxDepth : 2}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Including the data:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdges3}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_EDGES("
/// | + "'routeplanner', 'germanCity/Hamburg', {direction : 'outbound',"
/// | + "maxDepth : 2, includeData: true}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////