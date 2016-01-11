

@brief Define a directed relation.

`graph_module._directedRelation(relationName, fromVertexCollections, toVertexCollections)`

The *relationName* defines the name of this relation and references to the underlying edge collection.
The *fromVertexCollections* is an Array of document collections holding the start vertices.
The *toVertexCollections* is an Array of document collections holding the target vertices.
Relations are only allowed in the direction from any collection in *fromVertexCollections*
to any collection in *toVertexCollections*.

@PARAMS

@PARAM{relationName, string, required}
  The name of the edge collection where the edges should be stored.
  Will be created if it does not yet exist.

@PARAM{fromVertexCollections, array, required}
  One or a list of collection names. Source vertices for the edges
  have to be stored in these collections. Collections will be created if they do not exist.

@PARAM{toVertexCollections, array, required}
  One or a list of collection names. Target vertices for the edges
  have to be stored in these collections. Collections will be created if they do not exist.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphDirectedRelationDefinition}
  var graph_module = require("@arangodb/general-graph");
  graph_module._directedRelation("has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]);
@END_EXAMPLE_ARANGOSH_OUTPUT

