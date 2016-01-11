

@brief Delete one relation definition

`graph_module._deleteEdgeDefinition(edgeCollectionName, dropCollection)`

Deletes a relation definition defined by the edge collection of a graph. If the
collections defined in the edge definition (collection, from, to) are not used
in another edge definition of the graph, they will be moved to the orphanage.

@PARAMS

@PARAM{edgeCollectionName, string, required}
Name of edge collection in the relation definition.
@PARAM{dropCollection, boolean, optional}
Define if the edge collection should be dropped. Default false.

@EXAMPLES

Remove an edge definition but keep the edge collection:

@EXAMPLE_ARANGOSH_OUTPUT{general_graph__deleteEdgeDefinition}
  var graph_module = require("@arangodb/general-graph")
~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
  var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
  var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
  var graph = graph_module._create("myGraph", [ed1, ed2]);
  graph._deleteEdgeDefinition("myEC1");
  db._collection("myEC1");
~ db._drop("myEC1");
~ var blub = graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT

Remove an edge definition and drop the edge collection:

@EXAMPLE_ARANGOSH_OUTPUT{general_graph__deleteEdgeDefinitionWithDrop}
  var graph_module = require("@arangodb/general-graph")
~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
  var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
  var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
  var graph = graph_module._create("myGraph", [ed1, ed2]);
  graph._deleteEdgeDefinition("myEC1", true);
  db._collection("myEC1");
~ db._drop("myEC1");
~ var blub = graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT


