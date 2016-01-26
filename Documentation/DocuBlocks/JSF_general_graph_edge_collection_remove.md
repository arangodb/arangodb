

@brief Removes an edge in collection edgeCollectionName

`graph.edgeCollectionName.remove(edgeId, options)`

If this edge is used as a vertex by another edge, the other edge will be removed (recursively).

@PARAMS

@PARAM{edgeId, string, required}
*_id* attribute of the edge

@PARAM{options, object, optional}
See [collection documentation](../Documents/DocumentMethods.md)

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionRemove}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  graph.relation.save("female/alice", "female/diana", {_key: "aliceAndDiana"});
  db._exists("relation/aliceAndDiana")
  graph.relation.remove("relation/aliceAndDiana")
  db._exists("relation/aliceAndDiana")
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT


