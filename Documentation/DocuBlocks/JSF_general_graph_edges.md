

@brief Select some edges from the graph.

`graph._edges(examples)`

Creates an AQL statement to select a subset of the edges stored in the graph.
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

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgesUnfiltered}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  graph._edges().toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered edges:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgesFiltered}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  graph._edges({type: "married"}).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

