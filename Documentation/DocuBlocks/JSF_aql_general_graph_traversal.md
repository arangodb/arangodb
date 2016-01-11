////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_aql_general_graph_traversal
/// The GRAPH\_TRAVERSAL function traverses through the graph.
///
/// `GRAPH_TRAVERSAL (graphName, startVertexExample, direction, options)`
///
/// This function performs traversals on the given graph.
///
/// The complexity of this function strongly depends on the usage.
///
/// *Parameters*
/// * *graphName*          : The name of the graph as a string.
/// * *startVertexExample*        : An example for the desired
/// vertices (see [example](#short-explanation-of-the-example-parameter)).
/// * *direction*          : The direction of the edges as a string. Possible values
/// are *outbound*, *inbound* and *any* (default).
/// * *options*: Object containing optional options.
///
/// *Options*:
///  
/// * *strategy*: determines the visitation strategy. Possible values are 
/// *depthfirst* and *breadthfirst*. Default is *breadthfirst*.
/// * *order*: determines the visitation order. Possible values are 
/// *preorder* and *postorder*.
/// * *itemOrder*: determines the order in which connections returned by the 
/// expander will be processed. Possible values are *forward* and *backward*.
/// * *maxDepth*: if set to a value greater than *0*, this will limit the 
/// traversal to this maximum depth. 
/// * *minDepth*: if set to a value greater than *0*, all vertices found on 
/// a level below the *minDepth* level will not be included in the result.
/// * *maxIterations*: the maximum number of iterations that the traversal is 
/// allowed to perform. It is sensible to set this number so unbounded traversals 
/// will terminate at some point.
/// * *uniqueness*: an object that defines how repeated visitations of vertices should 
/// be handled. The *uniqueness* object can have a sub-attribute *vertices*, and a
/// sub-attribute *edges*. Each sub-attribute can have one of the following values:
///   * *"none"*: no uniqueness constraints
///   * *"path"*: element is excluded if it is already contained in the current path.
///    This setting may be sensible for graphs that contain cycles (e.g. A -> B -> C -> A).
///   * *"global"*: element is excluded if it was already found/visited at any 
///   point during the traversal.
/// * *filterVertices*  An optional array of example vertex documents that the traversal will treat specially.
///     If no examples are given, the traversal will handle all encountered vertices equally.
///     If one or many vertex examples are given, the traversal will exclude any non-matching vertex from the
///     result and/or not descend into it. Optionally, filterVertices can contain a string containing the name
///     of a user-defined AQL function that should be responsible for filtering.
///     If so, the AQL function is expected to have the following signature:
/// 
///     `function (config, vertex, path)`
///
///     If a custom AQL function is used for filterVertices, it is expected to return one of the following values:
///
///     * [ ]: Include the vertex in the result and descend into its connected edges
///     * [ "prune" ]: Will include the vertex in the result but not descend into its connected edges
///     * [ "exclude" ]: Will not include the vertex in the result but descend into its connected edges
///     * [ "prune", "exclude" ]: Will completely ignore the vertex and its connected edges
///
/// * *vertexFilterMethod:* Only useful in conjunction with filterVertices and if no user-defined AQL function is used.
///     If specified, it will influence how vertices are handled that don't match the examples in filterVertices:
///
///    * [ "prune" ]: Will include non-matching vertices in the result but not descend into them
///    * [ "exclude" ]: Will not include non-matching vertices in the result but descend into them
///    * [ "prune", "exclude" ]: Will completely ignore the vertex and its connected edges
///
/// @EXAMPLES
///
/// A route planner example, start a traversal from Hamburg :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversal1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL('routeplanner', 'germanCity/Hamburg'," +
/// | " 'outbound') RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, start a traversal from Hamburg with a max depth of 1
/// so only the direct neighbors of Hamburg are returned:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphTraversal2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_TRAVERSAL('routeplanner', 'germanCity/Hamburg'," +
/// | " 'outbound', {maxDepth : 1}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////