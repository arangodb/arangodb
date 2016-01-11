////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_save
/// @brief Creates an edge from vertex *from* to vertex *to* in collection edgeCollectionName
///
/// `graph.edgeCollectionName.save(from, to, data, options)`
///
/// @PARAMS
///
/// @PARAM{from, string, required}
/// *_id* attribute of the source vertex
///
/// @PARAM{to, string, required}
/// *_id* attribute of the target vertex
///
/// @PARAM{data, object, required}
/// JSON data of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Edges/README.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionSave1}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("male/bob", "female/alice", {type: "married", _key: "bobAndAlice"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// If the collections of *from* and *to* are not defined in an edge definition of the graph,
/// the edge will not be stored.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionSave2}
///   var examples = require("@arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   | graph.relation.save(
///   |  "relation/aliceAndBob",
///   |   "female/alice",
///      {type: "married", _key: "bobAndAlice"}); // xpError(ERROR_GRAPH_INVALID_EDGE)
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////