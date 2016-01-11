

@brief Add another edge definition to the graph

`graph._extendEdgeDefinitions(edgeDefinition)`

Extends the edge definitions of a graph. If an orphan collection is used in this
edge definition, it will be removed from the orphanage. If the edge collection of
the edge definition to add is already used in the graph or used in a different
graph with different *from* and/or *to* collections an error is thrown.

@PARAMS

@PARAM{edgeDefinition, object, required}
The relation definition to extend the graph

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{general_graph__extendEdgeDefinitions}
  var graph_module = require("@arangodb/general-graph")
~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
  var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
  var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
  var graph = graph_module._create("myGraph", [ed1]);
  graph._extendEdgeDefinitions(ed2);
~ var blub = graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT


