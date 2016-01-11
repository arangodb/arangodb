

@brief Define an undirected relation.

`graph_module._undirectedRelation(relationName, vertexCollections)`

Defines an undirected relation with the name *relationName* using the
list of *vertexCollections*. This relation allows the user to store
edges in any direction between any pair of vertices within the
*vertexCollections*.

@PARAMS

@PARAM{relationName, string, required}
  The name of the edge collection where the edges should be stored.
  Will be created if it does not yet exist.

@PARAM{vertexCollections, array, required}
  One or a list of collection names for which connections are allowed.
  Will be created if they do not exist.

@EXAMPLES

To define simple relation with only one vertex collection:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphUndirectedRelationDefinition1}
  var graph_module = require("@arangodb/general-graph");
  graph_module._undirectedRelation("friend", "user");
@END_EXAMPLE_ARANGOSH_OUTPUT

To define a relation between several vertex collections:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphUndirectedRelationDefinition2}
  var graph_module = require("@arangodb/general-graph");
  graph_module._undirectedRelation("marriage", ["female", "male"]);
@END_EXAMPLE_ARANGOSH_OUTPUT

