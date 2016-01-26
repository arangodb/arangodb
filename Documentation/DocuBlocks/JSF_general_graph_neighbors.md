

@brief Get all neighbors of the vertices defined by the example

`graph._neighbors(vertexExample, options)`

The function accepts an id, an example, a list of examples or even an empty
example as parameter for vertexExample.
The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
parameter vertexExamplex, *m* the average amount of neighbors and *x* the maximal depths.
Hence the default call would have a complexity of **O(n\*m)**;

@PARAMS

@PARAM{vertexExample, object, optional}
See [Definition of examples](#definition-of-examples)
@PARAM{options, object, optional}
An object defining further options. Can have the following values:
  * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
  * *edgeExamples*: Filter the edges, see [Definition of examples](#definition-of-examples)
  * *neighborExamples*: Filter the neighbor vertices, see [Definition of examples](#definition-of-examples)
  * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
      considered to be on the path.
  * *vertexCollectionRestriction* : One or a list of vertex-collection names that should be
      considered on the intermediate vertex steps.
  * *minDepth*: Defines the minimal number of intermediate steps to neighbors (default is 1).
  * *maxDepth*: Defines the maximal number of intermediate steps to neighbors (default is 1).

@EXAMPLES

A route planner example, all neighbors of capitals.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleNeighbors1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._neighbors({isCapital : true});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, all outbound neighbors of Hamburg.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleNeighbors2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._neighbors('germanCity/Hamburg', {direction : 'outbound', maxDepth : 2});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


