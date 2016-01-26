

@brief Get the
[betweenness](http://en.wikipedia.org/wiki/Betweenness_centrality)
of all vertices in the graph.

`graph._absoluteBetweenness(vertexExample, options)`

The complexity of the function is described
[here](../Aql/GraphOperations.md#the-complexity-of-the-shortest-path-algorithms).

@PARAMS

@PARAM{vertexExample, object, optional}
Filter the vertices, see [Definition of examples](#definition-of-examples)

@PARAM{options, object, optional}
An object defining further options. Can have the following values:
  * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
  * *weight*: The name of the attribute of the edges containing the weight.
  * *defaultWeight*: Only used with the option *weight*.
      If an edge does not have the attribute named as defined in option *weight* this default
      is used as weight.
      If no default is supplied the default would be positive infinity so the path and
      hence the betweeness can not be calculated.

@EXAMPLES

A route planner example, the absolute betweenness of all locations.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsBetweenness1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._absoluteBetweenness({});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the absolute betweenness of all locations.
This considers the actual distances.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsBetweenness2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._absoluteBetweenness({weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the absolute betweenness of all cities regarding only
outbound paths.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsBetweenness3}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var graph = examples.loadGraph("routeplanner");
  graph._absoluteBetweenness({direction : 'outbound', weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


