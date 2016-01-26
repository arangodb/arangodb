

@brief Remove a vertex collection from the graph

`graph._removeVertexCollection(vertexCollectionName, dropCollection)`

Removes a vertex collection from the graph.
Only collections not used in any relation definition can be removed.
Optionally the collection can be deleted, if it is not used in any other graph.

@PARAMS

@PARAM{vertexCollectionName, string, required}
Name of vertex collection.

@PARAM{dropCollection, boolean, optional}
If true the collection will be dropped if it is
not used in any other graph. Default: false.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{general_graph__removeVertexCollections}
  var graph_module = require("@arangodb/general-graph")
~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
  var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
  var graph = graph_module._create("myGraph", [ed1]);
  graph._addVertexCollection("myVC3", true);
  graph._addVertexCollection("myVC4", true);
  graph._orphanCollections();
  graph._removeVertexCollection("myVC3");
  graph._orphanCollections();
~ db._drop("myVC3");
~ var blub = graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT


