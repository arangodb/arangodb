

@brief Get the normalized
[closeness](http://en.wikipedia.org/wiki/Centrality#Closeness_centrality)
of graphs vertices.

`graph._closeness(options)`

Similar to [_absoluteCloseness](#absolutecloseness) but returns a normalized value.

The complexity of the function is described
[here](../Aql/GraphOperations.md#the-complexity-of-the-shortest-path-algorithms).

@EXAMPLES

A route planner example, the normalized closeness of all locations.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCloseness1}
var examples = require("@arangodb/graph-examples/example-graph.js");
var graph = examples.loadGraph("routeplanner");
graph._closeness();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the closeness of all locations.
This considers the actual distances.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCloseness2}
var examples = require("@arangodb/graph-examples/example-graph.js");
var graph = examples.loadGraph("routeplanner");
graph._closeness({weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the closeness of all cities regarding only
outbound paths.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCloseness3}
var examples = require("@arangodb/graph-examples/example-graph.js");
var graph = examples.loadGraph("routeplanner");
graph._closeness({direction : 'outbound', weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


