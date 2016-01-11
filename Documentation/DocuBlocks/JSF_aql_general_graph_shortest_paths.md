

The GRAPH\_SHORTEST\_PATH function returns all shortest paths of a graph.

`GRAPH_SHORTEST_PATH (graphName, startVertexExample, endVertexExample, options)`

This function determines all shortest paths in a graph identified by *graphName*.
If one wants to call this function to receive nearly all shortest paths for a
graph the option *algorithm* should be set to
[Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) to
increase performance.
If no algorithm is provided in the options the function chooses the appropriate
one (either [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
 or [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)) according to its parameters.
The length of a path is by default the amount of edges from one start vertex to
an end vertex. The option weight allows the user to define an edge attribute
representing the length.

The complexity of the function is described
[here](#the-complexity-of-the-shortest-path-algorithms).

*Parameters*

* *graphName*          : The name of the graph as a string.
* *startVertexExample* : An example for the desired start Vertices
  (see [example](#short-explanation-of-the-example-parameter)).
* *endVertexExample*   : An example for the desired
  end Vertices (see [example](#short-explanation-of-the-example-parameter)).
* *options* (optional) : An object containing the following options:
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
    (see [example](#short-explanation-of-the-example-parameter)).
  * *algorithm*                        : The algorithm to calculate
    the shortest paths. If both start and end vertex examples are empty
    [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) is
    used, otherwise the default is [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
  * *weight*                           : The name of the attribute of
    the edges containing the length as a string.
  * *defaultWeight*                    : Only used with the option *weight*.
    If an edge does not have the attribute named as defined in option *weight* this default
    is used as length.
    If no default is supplied the default would be positive Infinity so the path could
    not be calculated.
  * *stopAtFirstMatch*                 : Only useful if targetVertices is an example that matches 
    to more than one vertex. If so only the shortest path to
    the closest of these target vertices is returned.
    This flag is of special use if you have target pattern and
    you want to know which vertex with this pattern is matched first.
  * *includeData*                      : Triggers if only *_id*'s are returned (*false*, default)
    or if data is included for all objects as well (*true*)
    This will modify the content of *vertex*, *path.vertices*
    and *path.edges*. 

NOTE: Since version 2.6 we have included a new optional parameter *includeData*.
This parameter triggers if the result contains the real data object *true* or
it just includes the *_id* values *false*.
The default value is *false* as it yields performance benefits.

@EXAMPLES

A route planner example, shortest distance from all german to all french cities:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphShortestPaths1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| db._query("FOR e IN GRAPH_SHORTEST_PATH("
| + "'routeplanner', {}, {}, {" +
| "weight : 'distance', endVertexCollectionRestriction : 'frenchCity', " +
| "includeData: true, " +
| "startVertexCollectionRestriction : 'germanCity'}) RETURN [e.startVertex, e.vertex._id, " +
| "e.distance, LENGTH(e.paths)]"
).toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, shortest distance from Hamburg and Cologne to Lyon:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphShortestPaths2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| db._query("FOR e IN GRAPH_SHORTEST_PATH("
| +"'routeplanner', [{_id: 'germanCity/Cologne'},{_id: 'germanCity/Munich'}], " +
| "'frenchCity/Lyon', " +
| "{weight : 'distance'}) RETURN [e.startVertex, e.vertex, e.distance, LENGTH(e.paths)]"
).toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


