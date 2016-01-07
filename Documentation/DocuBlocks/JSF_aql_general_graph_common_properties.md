////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_aql_general_graph_common_properties
///
/// `GRAPH_COMMON_PROPERTIES (graphName, vertex1Example, vertex2Examples, options)`
///
/// The GRAPH\_COMMON\_PROPERTIES function returns a list of objects which have the id of
/// the vertices defined by *vertex1Example* as keys and a list of vertices defined by
/// *vertex21Example*, that share common properties as value. Notice that only the
/// vertex id and the matching attributes are returned in the result.
///
/// The complexity of this method is **O(n)** with *n* being the maximal amount of vertices
/// defined by the parameters vertexExamples.
///
/// *Parameters*
///
/// * *graphName*          : The name of the graph as a string.
/// * *vertex1Example*     : An example for the desired
/// vertices (see [example](#short-explanation-of-the-example-parameter)).
/// * *vertex2Example*     : An example for the desired
/// vertices (see [example](#short-explanation-of-the-example-parameter)).
/// * *options* (optional)    : An object containing the following options:
///   * *vertex1CollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered.
///   * *vertex2CollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered.
///   * *ignoreProperties* : One or multiple
///  attributes of a document that should be ignored, either a string or an array..
///
/// @EXAMPLES
///
/// A route planner example, all locations with the same properties:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphProperties1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_PROPERTIES("
/// | +"'routeplanner', {}, {}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all cities which share same properties except for population.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphProperties2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | db._query("FOR e IN GRAPH_COMMON_PROPERTIES("
/// | +"'routeplanner', {}, {}, {ignoreProperties: 'population'}) RETURN e"
/// ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////