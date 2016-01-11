

@brief Create a graph

`graph_module._create(graphName, edgeDefinitions, orphanCollections)`

The creation of a graph requires the name of the graph and a definition of its edges.

For every type of edge definition a convenience method exists that can be used to create a graph.
Optionally a list of vertex collections can be added, which are not used in any edge definition.
These collections are referred to as orphan collections within this chapter.
All collections used within the creation process are created if they do not exist.

@PARAMS

@PARAM{graphName, string, required}
Unique identifier of the graph

@PARAM{edgeDefinitions, array, optional}
List of relation definition objects

@PARAM{orphanCollections, array, optional}
List of additional vertex collection names

@EXAMPLES

Create an empty graph, edge definitions can be added at runtime:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraph}
  var graph_module = require("@arangodb/general-graph");
  graph = graph_module._create("myGraph");
~ graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT

Create a graph using an edge collection `edges` and a single vertex collection `vertices` 

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphSingle}
~ db._drop("edges");
~ db._drop("vertices");
  var graph_module = require("@arangodb/general-graph");
  var edgeDefinitions = [ { collection: "edges", "from": [ "vertices" ], "to" : [ "vertices" ] } ];
  graph = graph_module._create("myGraph", edgeDefinitions);
~ graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT

Create a graph with edge definitions and orphan collections:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraph2}
  var graph_module = require("@arangodb/general-graph");
| graph = graph_module._create("myGraph",
  [graph_module._relation("myRelation", ["male", "female"], ["male", "female"])], ["sessions"]);
~ graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT


