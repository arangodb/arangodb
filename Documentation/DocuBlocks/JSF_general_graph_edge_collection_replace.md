

@brief Replaces the data of an edge in collection edgeCollectionName

`graph.edgeCollectionName.replace(edgeId, data, options)`

@PARAMS

@PARAM{edgeId, string, required}
*_id* attribute of the edge

@PARAM{data, object, required}
JSON data of the edge

@PARAM{options, object, optional}
See [collection documentation](../Documents/DocumentMethods.md)

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionReplace}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  graph.relation.save("female/alice", "female/diana", {typo: "nose", _key: "aliceAndDiana"});
  graph.relation.replace("relation/aliceAndDiana", {type: "knows"});
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT


