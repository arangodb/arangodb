

The GRAPH\_DISTANCE\_TO function returns all paths and there distance within a graph.

`GRAPH_DISTANCE_TO (graphName, startVertexExample, endVertexExample, options)`

This function is a wrapper of [GRAPH\_SHORTEST\_PATH](#graphshortestpath).
It does not return the actual path but only the distance between two vertices.

@EXAMPLES

A route planner example, distance from all french to all german cities:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphDistanceTo1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| db._query("FOR e IN GRAPH_DISTANCE_TO("
| +"'routeplanner', {}, {}, { " +
| " weight : 'distance', endVertexCollectionRestriction : 'germanCity', " +
| "startVertexCollectionRestriction : 'frenchCity'}) RETURN [e.startVertex, e.vertex, " +
| "e.distance]"
).toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, distance from Hamburg and Cologne to Lyon:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphDistanceTo2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| db._query("FOR e IN GRAPH_DISTANCE_TO("
| + "'routeplanner', [{_id: 'germanCity/Cologne'},{_id: 'germanCity/Hamburg'}], " +
| "'frenchCity/Lyon', " +
| "{weight : 'distance'}) RETURN [e.startVertex, e.vertex, e.distance]"
).toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


