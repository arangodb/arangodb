

@brief Get the normalized
[betweenness](http://en.wikipedia.org/wiki/Betweenness_centrality)
of graphs vertices.

`graph_module._betweenness(options)`

Similar to [_absoluteBetweeness](#absolutebetweenness) but returns normalized values.

@EXAMPLES

A route planner example, the betweenness of all locations.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleBetweenness1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._betweenness();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the betweenness of all locations.
This considers the actual distances.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleBetweenness2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._betweenness({weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the betweenness of all cities regarding only
outbound paths.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleBetweenness3}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._betweenness({direction : 'outbound', weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


