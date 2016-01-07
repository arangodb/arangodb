////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_definitions
/// @brief Create a list of edge definitions to construct a graph.
///
/// `graph_module._edgeDefinitions(relation1, relation2, ..., relationN)`
///
/// The list of edge definitions of a graph can be managed by the graph module itself.
/// This function is the entry point for the management and will return the correct list.
///
/// @PARAMS
///
/// @PARAM{relationX, object, optional}
/// An object representing a definition of one relation in the graph
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeDefinitions}
///   var graph_module = require("@arangodb/general-graph");
///   directed_relation = graph_module._relation("lives_in", "user", "city");
///   undirected_relation = graph_module._relation("knows", "user", "user");
///   edgedefinitions = graph_module._edgeDefinitions(directed_relation, undirected_relation);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////