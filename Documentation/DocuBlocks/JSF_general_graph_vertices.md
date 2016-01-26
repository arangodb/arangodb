

@brief Select some vertices from the graph.

`graph._vertices(examples)`

Creates an AQL statement to select a subset of the vertices stored in the graph.
This is one of the entry points for the fluent AQL interface.
It will return a mutable AQL statement which can be further refined, using the
functions described below.
The resulting set of edges can be filtered by defining one or more *examples*.

@PARAMS

@PARAM{examples, object, optional}
See [Definition of examples](#definition-of-examples)

@EXAMPLES

In the examples the *toArray* function is used to print the result.
The description of this function can be found below.

To request unfiltered vertices:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphVerticesUnfiltered}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  graph._vertices().toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered vertices:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphVerticesFiltered}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  graph._vertices([{name: "Alice"}, {name: "Bob"}]).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

