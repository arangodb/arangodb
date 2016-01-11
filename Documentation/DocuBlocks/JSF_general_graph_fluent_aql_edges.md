

@brief Select all edges for the vertices selected before.

`graph_query.edges(examples)`

Creates an AQL statement to select all edges for each of the vertices selected
in the step before.
This will include *inbound* as well as *outbound* edges.
The resulting set of edges can be filtered by defining one or more *examples*.

The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
parameter vertexExamplex, *m* the average amount of edges of a vertex and *x* the maximal depths.
Hence the default call would have a complexity of **O(n\*m)**;

@PARAMS

@PARAM{examples, object, optional}
See [Definition of examples](#definition-of-examples)

@EXAMPLES

To request unfiltered edges:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesUnfiltered}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  query.edges().toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered edges by a single example:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesFilteredSingle}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  query.edges({type: "married"}).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered edges by multiple examples:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesFilteredMultiple}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  query.edges([{type: "married"}, {type: "friend"}]).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

