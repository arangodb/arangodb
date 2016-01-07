////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_common_properties
/// @brief Get the vertices of the graph that share common properties.
///
/// `graph._commonProperties(vertex1Example, vertex2Examples, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for vertex1Example and vertex2Example.
///
/// The complexity of this method is **O(n)** with *n* being the maximal amount of vertices
/// defined by the parameters vertexExamples.
///
/// @PARAMS
///
/// @PARAM{vertex1Examples, object, optional}
/// Filter the set of source vertices, see [Definition of examples](#definition-of-examples)
///
/// @PARAM{vertex2Examples, object, optional}
/// Filter the set of vertices compared to, see [Definition of examples](#definition-of-examples)
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *vertex1CollectionRestriction* : One or a list of vertex-collection names that should be
///       searched for source vertices.
///   * *vertex2CollectionRestriction* : One or a list of vertex-collection names that should be
///       searched for compare vertices.
///   * *ignoreProperties* : One or a list of attribute names of a document that should be ignored.
///
/// @EXAMPLES
///
/// A route planner example, all locations with the same properties:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleProperties1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._commonProperties({}, {});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all cities which share same properties except for population.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleProperties2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._commonProperties({}, {}, {ignoreProperties: 'population'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////