////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_save
/// @brief Create a new vertex in vertexCollectionName
///
/// `graph.vertexCollectionName.save(data)`
///
/// @PARAMS
///
/// @PARAM{data, object, required}
/// JSON data of vertex.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionSave}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.male.save({name: "Floyd", _key: "floyd"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////