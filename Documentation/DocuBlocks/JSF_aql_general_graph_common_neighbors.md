

The GRAPH\_COMMON\_NEIGHBORS function returns all common neighbors of the vertices
defined by the examples.

`GRAPH_COMMON_NEIGHBORS (graphName, vertex1Example, vertex2Examples,
optionsVertex1, optionsVertex2)`

This function returns the intersection of *GRAPH_NEIGHBORS(vertex1Example, optionsVertex1)*
and *GRAPH_NEIGHBORS(vertex2Example, optionsVertex2)*.
The complexity of this method is **O(n\*m^x)** with *n* being the maximal amount of vertices
defined by the parameters vertexExamples, *m* the average amount of neighbors and *x* the
maximal depths.
Hence the default call would have a complexity of **O(n\*m)**;

For parameter documentation read the documentation of
[GRAPH_NEIGHBORS](#graphneighbors).

@EXAMPLES

A route planner example, all common neighbors of capitals.

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphCommonNeighbors1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| db._query("FOR e IN GRAPH_COMMON_NEIGHBORS("
| +"'routeplanner', {isCapital : true}, {isCapital : true}) RETURN e"
).toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, all common outbound neighbors of Hamburg with any other location
which have a maximal depth of 2:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphCommonNeighbors2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| db._query("FOR e IN GRAPH_COMMON_NEIGHBORS("
| +"'routeplanner', 'germanCity/Hamburg', {}, {direction : 'outbound', maxDepth : 2}, "+
| "{direction : 'outbound', maxDepth : 2}) RETURN e"
).toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


