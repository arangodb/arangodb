

@EXAMPLE_ARANGOSH_OUTPUT{general_graph_create_graph_example1}
  var graph_module = require("@arangodb/general-graph");
  var edgeDefinitions = graph_module._edgeDefinitions();
  graph_module._extendEdgeDefinitions(edgeDefinitions, graph_module._relation("friend_of", "Customer", "Customer"));
| graph_module._extendEdgeDefinitions(
| edgeDefinitions, graph_module._relation(
  "has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]));
  graph_module._create("myStore", edgeDefinitions);
~ graph_module._drop("myStore");
~ db._drop("Electronics");
~ db._drop("Customer");
~ db._drop("Groceries");
~ db._drop("Company");
~ db._drop("has_bought");
~ db._drop("friend_of");
@END_EXAMPLE_ARANGOSH_OUTPUT


