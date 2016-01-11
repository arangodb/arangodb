////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_extend_edge_definitions
/// @brief Extend the list of edge definitions to construct a graph.
///
/// `graph_module._extendEdgeDefinitions(edgeDefinitions, relation1, relation2, ..., relationN)`
///
/// In order to add more edge definitions to the graph before creating
/// this function can be used to add more definitions to the initial list.
///
/// @PARAMS
///
/// @PARAM{edgeDefinitions, array, required}
/// A list of relation definition objects.
///
/// @PARAM{relationX, object, required}
/// An object representing a definition of one relation in the graph
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeDefinitionsExtend}
///   var graph_module = require("@arangodb/general-graph");
///   directed_relation = graph_module._relation("lives_in", "user", "city");
///   undirected_relation = graph_module._relation("knows", "user", "user");
///   edgedefinitions = graph_module._edgeDefinitions(directed_relation);
///   edgedefinitions = graph_module._extendEdgeDefinitions(undirected_relation);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////