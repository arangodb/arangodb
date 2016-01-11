

@brief Select all outbound edges for the vertices selected before.

`graph_query.outEdges(examples)`

Creates an AQL statement to select all *outbound* edges for each of the vertices selected
in the step before.
The resulting set of edges can be filtered by defining one or more *examples*.

@PARAMS

@PARAM{examples, object, optional}
See [Definition of examples](#definition-of-examples)

@EXAMPLES

To request unfiltered outbound edges:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesUnfiltered}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  query.outEdges().toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered outbound edges by a single example:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesFilteredSingle}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  query.outEdges({type: "married"}).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered outbound edges by multiple examples:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesFilteredMultiple}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  query.outEdges([{type: "married"}, {type: "friend"}]).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

