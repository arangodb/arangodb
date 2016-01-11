////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_aql_general_graph_traversal_tree
/// The GRAPH\_TRAVERSAL\_TREE function traverses through the graph.
///
/// `GRAPH_TRAVERSAL_TREE (graphName, startVertexExample, direction, connectName, options)`
/// This function creates a tree format from the result for a better visualization of
/// the path.
/// This function performs traversals on the given graph.
///
/// The complexity of this function strongly depends on the usage.
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *startVertexExample*         : An example for the desired
/// vertices (see [example](#short-explanation-of-the-example-parameter)).
/// * *direction*          : The direction of the edges as a string.
///  Possible values are *outbound*, *inbound* and *any* (default).
/// * *connectName*        : The result attribute which
///  contains the connection.
/// * *options* (optional) : An object containing options, see
///  [Graph Traversals](../Aql/GraphOperations.md#graphtraversal):
///
/// @EXAMPLES
///
/// A route planner example, start a traversal from Hamburg :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversalTree1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL_TREE('routeplanner', 'germanCity/Hamburg'," +
/// | " 'outbound', 'connnection') RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, start a traversal from Hamburg with a max depth of 1 so
///  only the direct neighbors of Hamburg are returned:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversalTree2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL_TREE('routeplanner', 'germanCity/Hamburg',"+
/// | " 'outbound', 'connnection', {maxDepth : 1}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////