

@brief Select all vertices connected to the edges selected before.

`graph_query.vertices(examples)`

Creates an AQL statement to select all vertices for each of the edges selected
in the step before.
This includes all vertices contained in *_from* as well as *_to* attribute of the edges.
The resulting set of vertices can be filtered by defining one or more *examples*.

@PARAMS

@PARAM{examples, object, optional}
See [Definition of examples](#definition-of-examples)

@EXAMPLES

To request unfiltered vertices:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesUnfiltered}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._edges({type: "married"});
  query.vertices().toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered vertices by a single example:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesFilteredSingle}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._edges({type: "married"});
  query.vertices({name: "Alice"}).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered vertices by multiple examples:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesFilteredMultiple}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._edges({type: "married"});
  query.vertices([{name: "Alice"}, {name: "Charly"}]).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

