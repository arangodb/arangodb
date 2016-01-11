

@brief Select all source vertices of the edges selected before.

`graph_query.fromVertices(examples)`

Creates an AQL statement to select the set of vertices where the edges selected
in the step before start at.
This includes all vertices contained in *_from* attribute of the edges.
The resulting set of vertices can be filtered by defining one or more *examples*.

@PARAMS

@PARAM{examples, object, optional}
See [Definition of examples](#definition-of-examples)

@EXAMPLES

To request unfiltered source vertices:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesUnfiltered}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._edges({type: "married"});
  query.fromVertices().toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered source vertices by a single example:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesFilteredSingle}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._edges({type: "married"});
  query.fromVertices({name: "Alice"}).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

To request filtered source vertices by multiple examples:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesFilteredMultiple}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("social");
  var query = graph._edges({type: "married"});
  query.fromVertices([{name: "Alice"}, {name: "Charly"}]).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

