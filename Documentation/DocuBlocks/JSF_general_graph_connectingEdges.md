

@brief Get all connecting edges between 2 groups of vertices defined by the examples

`graph._connectingEdges(vertexExample, vertexExample2, options)`

The function accepts an id, an example, a list of examples or even an empty
example as parameter for vertexExample.

@PARAMS

@PARAM{vertexExample1, object, optional}
See [Definition of examples](Functions.md#definition-of-examples)
@PARAM{vertexExample2, object, optional}
See [Definition of examples](Functions.md#definition-of-examples)
@PARAM{options, object, optional}
An object defining further options. Can have the following values:
  * *edgeExamples*: Filter the edges, see [Definition of examples](Functions.md#definition-of-examples)
  * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
      considered to be on the path.
  * *vertex1CollectionRestriction* : One or a list of vertex-collection names that should be
      considered on the intermediate vertex steps.
  * *vertex2CollectionRestriction* : One or a list of vertex-collection names that should be
      considered on the intermediate vertex steps.

@EXAMPLES

A route planner example, all connecting edges between capitals.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleConnectingEdges1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._getConnectingEdges({isCapital : true}, {isCapital : true});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


