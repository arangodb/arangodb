


* Create a graph

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo1}
  var graph_module = require("@arangodb/general-graph");
  var graph = graph_module._create("myGraph");
  graph;
~ graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT

* Add some vertex collections

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo2}
~ var graph_module = require("@arangodb/general-graph");
~ var graph = graph_module._create("myGraph");
  graph._addVertexCollection("shop");
  graph._addVertexCollection("customer");
  graph._addVertexCollection("pet");
  graph;
~ graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT

* Define relations on the

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo3}
~ var graph_module = require("@arangodb/general-graph");
~ var graph = graph_module._create("myGraph");
  var rel = graph_module._relation("isCustomer", ["shop"], ["customer"]);
  graph._extendEdgeDefinitions(rel);
  graph;
~ graph_module._drop("myGraph", true);
@END_EXAMPLE_ARANGOSH_OUTPUT


