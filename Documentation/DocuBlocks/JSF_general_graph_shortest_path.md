

@brief The _shortestPath function returns all shortest paths of a graph.

`graph._shortestPath(startVertexExample, endVertexExample, options)`

This function determines all shortest paths in a graph.
The function accepts an id, an example, a list of examples
or even an empty example as parameter for
start and end vertex. If one wants to call this function to receive nearly all
shortest paths for a graph the option *algorithm* should be set to
[Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
to increase performance.
If no algorithm is provided in the options the function chooses the appropriate
one (either [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
or [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)) according to its parameters.
The length of a path is by default the amount of edges from one start vertex to
an end vertex. The option weight allows the user to define an edge attribute
representing the length.

The complexity of the function is described
[here](../Aql/GraphOperations.md#the-complexity-of-the-shortest-path-algorithms).

@PARAMS

@PARAM{startVertexExample, object, optional}
An example for the desired start Vertices
(see [Definition of examples](#definition-of-examples)).

@PARAM{endVertexExample, object, optional}
An example for the desired
end Vertices (see [Definition of examples](#definition-of-examples)).

@PARAM{options, object, optional}
An object containing options, see below:
  * *direction*                        : The direction of the edges as a string.
  Possible values are *outbound*, *inbound* and *any* (default).
  * *edgeCollectionRestriction*        : One or multiple edge
  collection names. Only edges from these collections will be considered for the path.
  * *startVertexCollectionRestriction* : One or multiple vertex
  collection names. Only vertices from these collections will be considered as
  start vertex of a path.
  * *endVertexCollectionRestriction*   : One or multiple vertex
  collection names. Only vertices from these collections will be considered as
  end vertex of a path.
  * *edgeExamples*                     : A filter example for the
  edges in the shortest paths
  (see [example](#definition-of-examples)).
  * *algorithm*                        : The algorithm to calculate
  the shortest paths. If both start and end vertex examples are empty
  [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) is
  used, otherwise the default is [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)
  * *weight*                           : The name of the attribute of
  the edges containing the length as a string.
  * *defaultWeight*                    : Only used with the option *weight*.
  If an edge does not have the attribute named as defined in option *weight* this default
  is used as length.
  If no default is supplied the default would be positive Infinity so the path could
  not be calculated.

@EXAMPLES

A route planner example, shortest path from all german to all french cities:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleShortestPaths1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| g._shortestPath({}, {}, {weight : 'distance', endVertexCollectionRestriction : 'frenchCity',
  startVertexCollectionRestriction : 'germanCity'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, shortest path from Hamburg and Cologne to Lyon:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleShortestPaths2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| g._shortestPath([{_id: 'germanCity/Cologne'},{_id: 'germanCity/Munich'}], 'frenchCity/Lyon',
  {weight : 'distance'});
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


