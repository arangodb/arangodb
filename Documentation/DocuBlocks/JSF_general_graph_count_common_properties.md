////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_count_common_properties
/// @brief Get the amount of vertices of the graph that share common properties.
///
/// `graph._countCommonProperties(vertex1Example, vertex2Examples, options)`
///
/// Similar to [_commonProperties](#commonproperties) but returns count instead of
/// the objects.
///
/// @EXAMPLES
///
/// A route planner example, all locations with the same properties:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAmountProperties1}
/// var examples = require("@arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._countCommonProperties({}, {});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all German cities which share same properties except for population.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAmountProperties2}
/// var examples = require("@arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// | graph._countCommonProperties({}, {}, {vertex1CollectionRestriction : 'germanCity',
///   vertex2CollectionRestriction : 'germanCity' ,ignoreProperties: 'population'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////