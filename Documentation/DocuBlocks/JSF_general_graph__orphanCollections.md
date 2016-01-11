////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__orphanCollections
/// @brief Get all orphan collections
///
/// `graph._orphanCollections()`
///
/// Returns all vertex collections of the graph that are not used in any edge definition.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__orphanCollections}
///   var graph_module = require("@arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._addVertexCollection("myVC3", true);
///   graph._orphanCollections();
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////