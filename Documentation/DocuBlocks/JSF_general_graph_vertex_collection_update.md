////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_update
/// @brief Updates the data of a vertex in collection vertexCollectionName
///
/// `graph.vertexCollectionName.update(vertexId, data, options)`
///
/// @PARAMS
///
/// @PARAM{vertexId, string, required}
/// *_id* attribute of the vertex
///
/// @PARAM{data, object, required}
/// JSON data of vertex.
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionUpdate}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.female.save({name: "Lynda", _key: "linda"});
///   graph.female.update("female/linda", {name: "Linda", _key: "linda"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////