

@brief Modify an relation definition

`graph_module._editEdgeDefinition(edgeDefinition)`

Edits one relation definition of a graph. The edge definition used as argument will
replace the existing edge definition of the graph which has the same collection.
Vertex Collections of the replaced edge definition that are not used in the new
definition will transform to an orphan. Orphans that are used in this new edge
definition will be deleted from the list of orphans. Other graphs with the same edge
definition will be modified, too.

@PARAMS

@PARAM{edgeDefinition, object, required}
The edge definition to replace the existing edge
definition with the same attribute *collection*.

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{general_graph__editEdgeDefinition}
  var graph_module = require("@arangodb/general-graph")
~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
  var original = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
  var modified = graph_module._relation("myEC1", ["myVC2"], ["myVC3"]);
  var graph = graph_module._create("myGraph", [original]);
  graph._editEdgeDefinitions(modified);
~ var blub = graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT


