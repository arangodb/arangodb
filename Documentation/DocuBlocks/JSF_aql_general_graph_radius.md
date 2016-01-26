


`GRAPH_RADIUS (graphName, options)`

*The GRAPH\_RADIUS function returns the
[radius](http://en.wikipedia.org/wiki/Eccentricity_%28graph_theory%29)
of a graph.*

The complexity of the function is described
[here](#the-complexity-of-the-shortest-path-algorithms).

* *graphName*       : The name of the graph as a string.
* *options*     : An object containing the following options:
  * *direction*     : The direction of the edges.
Possible values are *outbound*, *inbound* and *any* (default).
  * *algorithm*     : The algorithm to calculate the shortest paths as a string. Possible
values are [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
 (default) and [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
  * *weight*           : The name of the attribute of
the edges containing the length.
  * *defaultWeight*    : Only used with the option *weight*.
If an edge does not have the attribute named as defined in option *weight* this default
is used as length.
If no default is supplied the default would be positive Infinity so the path and
hence the eccentricity can not be calculated.

@EXAMPLES

A route planner example, the radius of the graph.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphRadius1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
  db._query("RETURN GRAPH_RADIUS('routeplanner')").toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the radius of the graph.
This considers the actual distances.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphRadius2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
  db._query("RETURN GRAPH_RADIUS('routeplanner', {weight : 'distance'})").toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, the radius of the graph regarding only
outbound paths.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphRadius3}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| db._query("RETURN GRAPH_RADIUS("
| + "'routeplanner', {direction : 'outbound', weight : 'distance'})"
).toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


