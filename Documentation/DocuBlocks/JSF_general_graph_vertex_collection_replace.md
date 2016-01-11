

@brief Replaces the data of a vertex in collection vertexCollectionName

`graph.vertexCollectionName.replace(vertexId, data, options)`

@PARAMS

@PARAM{vertexId, string, required}
*_id* attribute of the vertex

@PARAM{data, object, required}
JSON data of vertex.

@PARAM{options, object, optional}
See [collection documentation](../Documents/DocumentMethods.md)

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionReplace}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  graph.male.save({neym: "Jon", _key: "john"});
  graph.male.replace("male/john", {name: "John"});
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT


