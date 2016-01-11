

@brief Get the normalized
[eccentricity](http://en.wikipedia.org/wiki/Distance_%28graph_theory%29)
of the vertices defined by the examples.

`graph._eccentricity(vertexExample, options)`

Similar to [_absoluteEccentricity](#absoluteeccentricity) but returns a normalized result.

The complexity of the function is described
[here](../Aql/GraphOperations.md#the-complexity-of-the-shortest-path-algorithms).

@EXAMPLES

A route planner example, the eccentricity of all locations.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleEccentricity2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._eccentricity();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the weighted eccentricity.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleEccentricity3}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._eccentricity({weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


