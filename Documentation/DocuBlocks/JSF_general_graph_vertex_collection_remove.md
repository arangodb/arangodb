

@brief Removes a vertex in collection *vertexCollectionName*

`graph.vertexCollectionName.remove(vertexId, options)`

Additionally removes all ingoing and outgoing edges of the vertex recursively
(see [edge remove](#remove-an-edge)).

@PARAMS

@PARAM{vertexId, string, required}
*_id* attribute of the vertex

@PARAM{options, object, optional}
See [collection documentation](../Documents/DocumentMethods.md)

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionRemove}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  graph.male.save({name: "Kermit", _key: "kermit"});
  db._exists("male/kermit")
  graph.male.remove("male/kermit")
  db._exists("male/kermit")
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT


