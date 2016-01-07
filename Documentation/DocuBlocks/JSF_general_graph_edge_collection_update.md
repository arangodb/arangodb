////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_update
/// @brief Updates the data of an edge in collection edgeCollectionName
///
/// `graph.edgeCollectionName.update(edgeId, data, options)`
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @PARAM{data, object, required}
/// JSON data of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionUpdate}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("female/alice", "female/diana", {type: "knows", _key: "aliceAndDiana"});
///   graph.relation.update("relation/aliceAndDiana", {type: "quarrelled", _key: "aliceAndDiana"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////