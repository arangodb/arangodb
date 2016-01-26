

@brief Get the
[diameter](http://en.wikipedia.org/wiki/Eccentricity_%28graph_theory%29)
of a graph.

`graph._diameter(graphName, options)`

The complexity of the function is described
[here](../Aql/GraphOperations.md#the-complexity-of-the-shortest-path-algorithms).

@PARAMS

@PARAM{options, object, optional}
An object defining further options. Can have the following values:
  * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
  * *algorithm*: The algorithm to calculate the shortest paths, possible values are
      [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
      [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
  * *weight*: The name of the attribute of the edges containing the weight.
  * *defaultWeight*: Only used with the option *weight*.
      If an edge does not have the attribute named as defined in option *weight* this default
      is used as weight.
      If no default is supplied the default would be positive infinity so the path and
      hence the radius can not be calculated.

@EXAMPLES

A route planner example, the diameter of the graph.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDiameter1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._diameter();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the diameter of the graph.
This considers the actual distances.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDiameter2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._diameter({weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the diameter of the graph regarding only
outbound paths.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDiameter3}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._diameter({direction : 'outbound', weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


