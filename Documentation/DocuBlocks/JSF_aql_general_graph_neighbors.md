

The GRAPH\_NEIGHBORS function returns all neighbors of vertices.

`GRAPH_NEIGHBORS (graphName, vertexExample, options)`

By default only the direct neighbors (path length equals 1) are returned, but one can define
the range of the path length to the neighbors with the options *minDepth* and *maxDepth*.
The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
parameter vertexExamplex, *m* the average amount of neighbors and *x* the maximal depths.
Hence the default call would have a complexity of **O(n\*m)**;

*Parameters*

* *graphName*          : The name of the graph as a string.
* *vertexExample*      : An example for the desired
  vertices (see [example](#short-explanation-of-the-example-parameter)).
* *options*            : An object containing the following options:
  * *direction*                        : The direction
    of the edges. Possible values are *outbound*, *inbound* and *any* (default).
  * *edgeExamples*                     : A filter example for the edges to
    the neighbors (see [example](#short-explanation-of-the-example-parameter)).
  * *neighborExamples*                 : An example for the desired neighbors
    (see [example](#short-explanation-of-the-example-parameter)).
  * *edgeCollectionRestriction*        : One or multiple edge
  collection names. Only edges from these collections will be considered for the path.
  * *vertexCollectionRestriction* : One or multiple vertex
    collection names. Only vertices from these collections will be contained in the
  result. This does not effect vertices on the path.
  * *minDepth*                         : Defines the minimal
    depth a path to a neighbor must have to be returned (default is 1).
  * *maxDepth*                         : Defines the maximal
    depth a path to a neighbor must have to be returned (default is 1).
  * *maxIterations*: the maximum number of iterations that the traversal is
    allowed to perform. It is sensible to set this number so unbounded traversals
    will terminate at some point.
  * *includeData* is a boolean value to define if the returned documents should be extracted 
    instead of returning their ids only. The default is *false*.

Note: in ArangoDB versions prior to 2.6 *NEIGHBORS* returned the array of neighbor vertices with 
all attributes and not just the vertex ids. To return to the same behavior, set the *includeData*
option to *true* in 2.6 and above.

@EXAMPLES

A route planner example, all neighbors of locations with a distance of either
700 or 600.:

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphNeighbors1}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| db._query("FOR e IN GRAPH_NEIGHBORS("
| +"'routeplanner', {}, {edgeExamples : [{distance: 600}, {distance: 700}]}) RETURN e"
).toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT

A route planner example, all outbound neighbors of Hamburg with a maximal depth of 2 :

@EXAMPLE_ARANGOSH_OUTPUT{generalGraphNeighbors2}
  var examples = require("@arangodb/graph-examples/example-graph.js");
  var g = examples.loadGraph("routeplanner");
| db._query("FOR e IN GRAPH_NEIGHBORS("
| +"'routeplanner', 'germanCity/Hamburg', {direction : 'outbound', maxDepth : 2}) RETURN e"
).toArray();
~ examples.dropGraph("routeplanner");
@END_EXAMPLE_ARANGOSH_OUTPUT


