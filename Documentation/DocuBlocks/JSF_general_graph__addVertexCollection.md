////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__addVertexCollection
/// @brief Add a vertex collection to the graph
///
/// `graph._addVertexCollection(vertexCollectionName, createCollection)`
///
/// Adds a vertex collection to the set of orphan collections of the graph. If the
/// collection does not exist, it will be created. If it is already used by any edge
/// definition of the graph, an error will be thrown.
///
/// @PARAMS
///
/// @PARAM{vertexCollectionName, string, required}
/// Name of vertex collection.
///
/// @PARAM{createCollection, boolean, optional}
/// If true the collection will be created if it does not exist. Default: true.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__addVertexCollection}
///   var graph_module = require("@arangodb/general-graph");
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._addVertexCollection("myVC3", true);
/// ~ db._drop("myVC3");
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////