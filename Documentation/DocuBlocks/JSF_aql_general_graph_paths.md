

The GRAPH\_PATHS function returns all paths of a graph.

`GRAPH_PATHS (graphName, options)`

The complexity of this method is **O(n\*n\*m)** with *n* being the amount of vertices in
the graph and *m* the average amount of connected edges;

*Parameters*

* *graphName*     : The name of the graph as a string.
* *options*     : An object containing the following options:
  * *direction*        : The direction of the edges. Possible values are *any*,
*inbound* and *outbound* (default).
  * *followCycles* (optional) : If set to *true* the query follows cycles in the graph,
default is false.
  * *minLength* (optional)     : Defines the minimal length a path must
have to be returned (default is 0).
  * *maxLength* (optional)     : Defines the maximal length a path must
have to be returned (default is 10).

@EXAMPLES

Return all paths of the graph "social":

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphPaths}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("social");
  db._query("RETURN GRAPH_PATHS('social')").toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

Return all inbound paths of the graph "social" with a maximal
length of 1 and a minimal length of 2:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphPaths2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("social");
| db._query(
| "RETURN GRAPH_PATHS('social', {direction : 'inbound', minLength : 1, maxLength :  2})"
  ).toArray();
~ examples.dropGraph("social");
@END_EXAMPLE_ARANGOSH_OUTPUT

