# Migrating GRAPH_* Functions from 2.8 or earlier to 3.0

## Problem

With the release of 3.0 all GRAPH functions have been dropped from AQL in favor of a more
native integration of graph features into the query language. I have used the old graph
functions and want to upgrade to 3.0.

Graph functions covered in this recipe:

* GRAPH_COMMON_NEIGHBORS
* GRAPH_COMMON_PROPERTIES
* GRAPH_DISTANCE_TO
* GRAPH_EDGES
* GRAPH_NEIGHBORS
* GRAPH_TRAVERSAL
* GRAPH_TRAVERSAL_TREE
* GRAPH_SHORTEST_PATH
* GRAPH_PATHS
* GRAPH_VERTICES

## Solution 1 Quick and Dirty (not recommended)

### When to use this solution

I am not willing to invest a lot if time into the upgrade process and i am
willing to surrender some performance in favor of less effort.
Some constellations may not work with this solution due to the nature of
user-defined functions.
Especially check for AQL queries that do both modifications
and `GRAPH_*` functions.

### Registering user-defined functions

This step has to be executed once on ArangoDB for every database we are using.

We connect to `arangodb` with `arangosh` to issue the following commands two:

```js
var graphs = require("@arangodb/general-graph");
graphs._registerCompatibilityFunctions();
```

These have registered all old `GRAPH_*` functions as user-defined functions again, with the prefix `arangodb::`.

### Modify the application code

Next we have to go through our application code and replace all calls to `GRAPH_*` by `arangodb::GRAPH_*`.
Now run a testrun of our application and check if it worked.
If it worked we are ready to go.

### Important Information

The user defined functions will call translated subqueries (as described in Solution 2).
The optimizer does not know anything about these subqueries beforehand and cannot optimize the whole plan.
Also there might be read/write constellations that are forbidden in user-defined functions, therefore
a "really" translated query may work while the user-defined function work around may be rejected.

## Solution 2 Translating the queries (recommended)

### When to use this solution

I am willing to invest some time on my queries in order to get
maximum performance, full query optimization and a better
control of my queries. No forcing into the old layout
any more.

### Before you start

If you are using `vertexExamples` which are not only `_id` strings do not skip
the GRAPH_VERTICES section, because it will describe how to translate them to
AQL. All graph functions using a vertexExample are identical to executing a
GRAPH_VERTICES before and using it's result as start point.
Example with NEIGHBORS:

```
FOR res IN GRAPH_NEIGHBORS(@graph, @myExample) RETURN res
```

Is identical to:

```
FOR start GRAPH_VERTICES(@graph, @myExample)
  FOR res IN GRAPH_NEIGHBORS(@graph, start) RETURN res
```

All non GRAPH_VERTICES functions will only explain the transformation for a single input document's `_id`.

### Options used everywhere

#### Option edgeCollectionRestriction

In order to use edge Collection restriction we just use the feature that the traverser
can walk over a list of edge collections directly. So the edgeCollectionRestrictions
just form this list (exampleGraphEdges):

```
// OLD
[..] FOR e IN GRAPH_EDGES(@graphName, @startId, {edgeCollectionRestriction: [edges1, edges2]}) RETURN e

// NEW
[..] FOR v, e IN ANY @startId edges1, edges2 RETURN DISTINCT e._id
```

Note: The `@graphName` bindParameter is not used anymore and probably has to be removed from the query.

#### Option includeData

If we use the option includeData we simply return the object directly instead of only the _id

Example GRAPH_EDGES:

```
// OLD
[..] FOR e IN GRAPH_EDGES(@graphName, @startId, {includeData: true}) RETURN e

// NEW
[..] FOR v, e IN ANY @startId GRAPH @graphName RETURN DISTINCT e
```

#### Option direction

The direction has to be placed before the start id.
Note here: The direction has to be placed as Word it cannot be handed in via a bindParameter
anymore:

```
// OLD
[..] FOR e IN GRAPH_EDGES(@graphName, @startId, {direction: 'inbound'}) RETURN e

// NEW
[..] FOR v, e IN INBOUND @startId GRAPH @graphName RETURN DISTINCT e._id
```

#### Options minDepth, maxDepth

If we use the options minDepth and maxDepth (both default 1 if not set) we can simply
put them in front of the direction part in the Traversal statement.

Example GRAPH_EDGES:

```
// OLD
[..] FOR e IN GRAPH_EDGES(@graphName, @startId, {minDepth: 2, maxDepth: 4}) RETURN e

// NEW
[..] FOR v, e IN 2..4 ANY @startId GRAPH @graphName RETURN DISTINCT e._id
```

#### Option maxIteration

The option `maxIterations` is removed without replacement.
Your queries are now bound by main memory not by an arbitrary number of iterations.

### GRAPH_VERTICES

First we have to branch on the example.
There we have three possibilities:

1. The example is an `_id` string.
2. The example is `null` or `{}`.
3. The example is a non empty object or an array.

#### Example is '_id' string

This is the easiest replacement. In this case we simply replace the function with a call to `DOCUMENT`:

```
// OLD
[..] GRAPH_VERTICES(@graphName, @idString) [..] 

// NEW
[..] DOCUMENT(@idString) [..] 
```

NOTE: The `@graphName` is not required anymore, we may have to adjust bindParameters.

The AQL graph features can work with an id directly, no need to call `DOCUMENT` before if we just need this to find a starting point.

#### Example is `null` or the empty object

This case means we use all documents from the graph.
Here we first have to now the vertex collections of the graph.

1. If we only have one collection (say `vertices`) we can replace it with a simple iteration over this collection:

```
// OLD
[..] FOR v IN GRAPH_VERTICES(@graphName, {}) [..] 

// NEW
[..] FOR v IN vertices [..] 
````

NOTE: The `@graphName` is not required anymore, we may have to adjust bindParameters.


2. We have more than one collection. This is the unfortunate case for a general replacement.
So in the general replacement we assume we do not want to exclude any of the collections in
the graph. Than we unfortunately have to form a `UNION`over all these collections.
Say our graph has the vertex collections `vertices1`, `vertices2`, `vertices3` we create a sub-query for
a single collection for each of them and wrap them in a call to `UNION`.

```
// OLD
[..] FOR v IN GRAPH_VERTICES(@graphName, {}) [..] 

// NEW
[..]
FOR v IN UNION( // We start a UNION
  (FOR v IN vertices1 RETURN v), // For each vertex collection
  (FOR v IN vertices2 RETURN v), // we create the same subquery
  (FOR v IN vertices3 RETURN v)
) // Finish with the UNION
[..] 
````

NOTE: If you have any more domain knowledge of your graph apply it at this point to identify which
collections are actually relevant as this `UNION` is a rather expensive operation.

If we use the option `vertexCollectionRestriction` in the original query. The `UNION` has to be formed
by the collections in this restriction instead of ALL collections.

#### Example is a non-empty object

First we follow the instructions for the empty object above.
In this section we will just focus on a single collection `vertices`, the UNION for multiple collections
is again wrapped around a subquery for each of these collections built in the following way.

Now we have to transform the example into an AQL `FILTER` statement.
Therefore we take all top-level attributes of the example and do an equal comparison with their values.
All of these comparisons are joined with an `AND` because the all have to be fulfilled.

Example:

```
// OLD
[..] FOR v IN GRAPH_VERTICES(@graphName, {foo: 'bar', the: {answer: 42}}}) [..] 

// NEW
[..] FOR v IN vertices
  FILTER v.foo == 'bar' // foo: bar
  AND    v.the == {answer: 42} //the: {answer: 42}
[..] 
```

#### Example is an array

The idea transformation is almost identical to a single non-empty object.
For each element in the array we create the filter conditions and than we
`OR`-combine them (mind the brackets):

```
// OLD
[..] FOR v IN GRAPH_VERTICES(@graphName, [{foo: 'bar', the: {answer: 42}}, {foo: 'baz'}])) [..] 

// NEW
[..] FOR v IN vertices
  FILTER (v.foo == 'bar' // foo: bar
    AND   v.the == {answer: 42}) //the: {answer: 42}
  OR (v.foo == 'baz')
[..] 
```

### GRAPH_EDGES

The GRAPH_EDGES can be simply replaced by a call to the AQL traversal.

#### No options

The default options did use a direction `ANY` and returned a distinct result of the edges.
Also it did just return the edges `_id` value.

```
// OLD
[..] FOR e IN GRAPH_EDGES(@graphName, @startId) RETURN e

// NEW
[..] FOR v, e IN ANY @startId GRAPH @graphName RETURN DISTINCT e._id
```

#### Option edgeExamples.

See `GRAPH_VERTICES` on how to transform examples to AQL FILTER. Apply the filter on the edge variable `e`.

### GRAPH_NEIGHBORS

The GRAPH_NEIGHBORS is a breadth-first-search on the graph with a global unique check for vertices. So we can replace it by a an AQL traversal with these options.

#### No options

The default options did use a direction `ANY` and returned a distinct result of the neighbors.
Also it did just return the neighbors `_id` value.

```
// OLD
[..] FOR n IN GRAPH_NEIGHBORS(@graphName, @startId) RETURN n

// NEW
[..] FOR n IN ANY @startId GRAPH @graphName OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN n
```

#### Option neighborExamples

See `GRAPH_VERTICES` on how to transform examples to AQL FILTER. Apply the filter on the neighbor variable `n`.

#### Option edgeExamples

See `GRAPH_VERTICES` on how to transform examples to AQL FILTER. Apply the filter on the edge variable `e`.

However this is a bit more complicated as it interferes with the global uniqueness check.
For edgeExamples it is sufficent when any edge pointing to the neighbor matches the filter. Using `{uniqueVertices: 'global'}` first picks any edge randomly. Than it checks against this edge only.
If we know there are no vertex pairs with multiple edges between them we can use the simple variant which is save:

```
// OLD
[..] FOR n IN GRAPH_NEIGHBORS(@graphName, @startId, {edgeExample: {label: 'friend'}}) RETURN e

// NEW
[..] FOR n, e IN ANY @startId GRAPH @graphName OPTIONS {bfs: true, uniqueVertices: 'global'} FILTER e.label == 'friend' RETURN n._id
```

If there may be multiple edges between the same pair of vertices we have to make the distinct check ourselfes and cannot rely on the traverser doing it correctly for us:

```
// OLD
[..] FOR n IN GRAPH_NEIGHBORS(@graphName, @startId, {edgeExample: {label: 'friend'}}) RETURN e

// NEW
[..] FOR n, e IN ANY @startId GRAPH @graphName OPTIONS {bfs: true} FILTER e.label == 'friend' RETURN DISTINCT n._id
```

#### Option vertexCollectionRestriction

If we use the vertexCollectionRestriction we have to postFilter the neighbors based on their collection. Therefore we can make use of the function `IS_SAME_COLLECTION`:

```
// OLD
[..] FOR n IN GRAPH_NEIGHBORS(@graphName, @startId, {vertexCollectionRestriction: ['vertices1', 'vertices2']}) RETURN e

// NEW
[..] FOR n IN ANY @startId GRAPH @graphName OPTIONS {bfs: true, uniqueVertices: true} FILTER IS_SAME_COLLECTION(vertices1, n) OR IS_SAME_COLLECTION(vertices2, n) RETURN DISTINCT n._id
```

### GRAPH_COMMON_NEIGHBORS

`GRAPH_COMMON_NEIGHBORS` is defined as two `GRAPH_NEIGHBORS` queries and than forming the `INTERSECTION` of both queries.
How to translate the options please refer to `GRAPH_NEIGHBORS`.
Finally we have to build the old result format `{left, right, neighbors}`.
If you just need parts of the result you can adapt this query to your specific needs.

```
// OLD
FOR v IN GRAPH_COMMON_NEIGHBORS(@graphName, 'vertices/1' , 'vertices/2',  {direction : 'any'}) RETURN v

// NEW
LET n1 = ( // Neighbors for vertex1Example
  FOR n IN ANY 'vertices/1' GRAPH 'graph' OPTIONS {bfs: true, uniqueVertices: "global"} RETURN n._id
  )
LET n2 = ( // Neighbors for vertex2Example
  FOR n IN ANY 'vertices/2' GRAPH 'graph' OPTIONS {bfs: true, uniqueVertices: "global"} RETURN n._id
  )
LET common = INTERSECTION(n1, n2) // Get the intersection
RETURN { // Produce the original result
  left: 'vertices/1',
  right: 'vertices/2,
  neighbors: common
}
```

NOTE: If you are using examples instead of `_ids` you have to add a filter to make sure that the left is not equal to the right start vertex.
To give you an example with a single vertex collection `vertices`, the replacement would look like this:

```
// OLD
FOR v IN GRAPH_COMMON_NEIGHBORS(@graphName, {name: "Alice"}, {name: "Bob"}) RETURN v

// NEW
FOR left IN vertices
  FILTER left.name == "Alice"
  LET n1 = (FOR n IN ANY left GRAPH 'graph' OPTIONS {bfs: true, uniqueVertices: "global"} RETURN n._id)
  FOR right IN vertices
    FILTER right.name == "Bob"
    FILTER right != left // Make sure left is not identical to right
    LET n2 = (FOR n IN ANY right GRAPH 'graph' OPTIONS {bfs: true, uniqueVertices: "global"} RETURN n._id)
    LET neighbors = INTERSECTION(n1, n2)
    FILTER LENGTH(neighbors) > 0 // Only pairs with shared neighbors should be returned
    RETURN {left: left._id, right: right._id, neighbors: neighbors}
```

### GRAPH_PATHS

This function computes all paths of the entire graph (with a given minDepth and maxDepth) as you can imagine this feature is extremely expensive and should never be used.
However paths can again be replaced by AQL traversal.
Assume we only have one vertex collection `vertices` again.

#### No options
By default paths of length 0 to 10 are returned. And circles are not followed.

```
// OLD
RETURN GRAPH_PATHS('graph')

// NEW
FOR start IN vertices
FOR v, e, p IN 0..10 OUTBOUND start GRAPH 'graph' RETURN {source: start, destination: v, edges: p.edges, vertices: p.vertices}
```

#### followCycles

If this option is set we have to modify the options of the traversal by modifying the `uniqueEdges` property:

```
// OLD
RETURN GRAPH_PATHS('graph', {followCycles: true})

// NEW
FOR start IN vertices
FOR v, e, p IN 0..10 OUTBOUND start GRAPH 'graph' OPTIONS {uniqueEdges: 'none'} RETURN {source: start, destination: v, edges: p.edges, vertices: p.vertices}
```

### GRAPH_COMMON_PROPERTIES

This feature involves several full-collection scans and therefore is extremely expensive.
If you really need it you can transform it with the help of `ATTRIBUTES`, `KEEP` and `ZIP`.

#### Start with single _id

```
// OLD
RETURN GRAPH_COMMON_PROPERTIES('graph', "vertices/1", "vertices/2")

// NEW
LET left = DOCUMENT("vertices/1") // get one document
LET right = DOCUMENT("vertices/2") // get the other one
LET shared = (FOR a IN ATTRIBUTES(left) // find all shared attributes
               FILTER left[a] == right[a]
                 OR a == '_id' // always include _id
                 RETURN a)
FILTER LENGTH(shared) > 1 // Return them only if they share an attribute
RETURN ZIP([left._id], [KEEP(right, shared)]) // Build the result
```

#### Start with vertexExamples

Again we assume we only have a single collection `vertices`.
We have to transform the examples into filters. Iterate
over vertices to find all left documents.
For each left document iterate over vertices again
to find matching right documents.
And return the shared attributes as above:

```
// OLD
RETURN GRAPH_COMMON_PROPERTIES('graph', {answer: 42}, {foo: "bar"})

// NEW
FOR left IN vertices
  FILTER left.answer == 42
  LET commons = (
    FOR right IN vertices
      FILTER right.foo == "bar"
      FILTER left != right
      LET shared = (FOR a IN ATTRIBUTES(left) 
                     FILTER left[a] == right[a]
                     OR a == '_id'
                       RETURN a)
      FILTER LENGTH(shared) > 1
      RETURN KEEP(right, shared))
  FILTER LENGTH(commons) > 0
  RETURN ZIP([left._id], [commons])
```


### GRAPH_SHORTEST_PATH

A shortest path computation is now done via the new SHORTEST_PATH AQL statement.

#### No options

```
// OLD
FOR p IN GRAPH_SHORTEST_PATH(@graphName, @startId, @targetId, {direction : 'outbound'}) RETURN p

// NEW
LET p = ( // Run one shortest Path
  FOR v, e IN OUTBOUND SHORTEST_PATH @startId TO @targetId GRAPH @graphName
  // We return objects with vertex, edge and weight for each vertex on the path
  RETURN {vertex: v, edge: e, weight: (IS_NULL(e) ? 0 : 1)}
)
FILTER LENGTH(p) > 0 // We only want shortest paths that actually exist
RETURN { // We rebuild the old format
  vertices: p[*].vertex,
  edges: p[* FILTER CURRENT.e != null].edge,
  distance: SUM(p[*].weight)
}
```

#### Options weight and defaultWeight

The new AQL SHORTEST_PATH offers the options `weightAttribute` and `defaultWeight`.

```
// OLD
FOR p IN GRAPH_SHORTEST_PATH(@graphName, @startId, @targetId, {direction : 'outbound', weight: "weight", defaultWeight: 80}) RETURN p

// NEW
LET p = ( // Run one shortest Path
  FOR v, e IN OUTBOUND SHORTEST_PATH @startId TO @targetId GRAPH @graphName
  // We return objects with vertex, edge and weight for each vertex on the path
  RETURN {vertex: v, edge: e, weight: (IS_NULL(e) ? 0 : (IS_NUMBER(e.weight) ? e.weight : 80))}
)
FILTER LENGTH(p) > 0 // We only want shortest paths that actually exist
RETURN { // We rebuild the old format
  vertices: p[*].vertex,
  edges: p[* FILTER CURRENT.e != null].edge,
  distance: SUM(p[*].weight) // We have to recompute the distance if we need it
}
```


### GRAPH_DISTANCE_TO

Graph distance to only differs by the result format from `GRAPH_SHORTEST_PATH`.
So we follow the transformation for `GRAPH_SHORTEST_PATH`, remove some unnecessary parts,
and change the return format

```
// OLD
FOR p IN GRAPH_DISTANCE_TO(@graphName, @startId, @targetId, {direction : 'outbound'}) RETURN p

// NEW
LET p = ( // Run one shortest Path
  FOR v, e IN OUTBOUND SHORTEST_PATH @startId TO @targetId GRAPH @graphName
  // DIFFERENCE we only return the weight for each edge on the path
  RETURN IS_NULL(e) ? 0 : 1} 
)
FILTER LENGTH(p) > 0 // We only want shortest paths that actually exist
RETURN { // We rebuild the old format
  startVertex: @startId,
  vertex: @targetId,
  distance: SUM(p[*].weight)
}
```

### GRAPH_TRAVERSAL and GRAPH_TRAVERSAL_TREE

These have been removed and should be replaced by the
[native AQL traversal](https://docs.arangodb.com/3/Manual/Graphs/Traversals/index.html).
There are many potential solutions using the new syntax, but they largely depend
on what exactly you are trying to achieve and would go beyond the scope of this
cookbook. Here is one example how to do the transition, using the
[world graph](https://docs.arangodb.com/3/Manual/Graphs/index.html#the-world-graph)
as data:

In 2.8, it was possible to use `GRAPH_TRAVERSAL()` together with a custom visitor
function to find leaf nodes in a graph. Leaf nodes are vertices that have inbound
edges, but no outbound edges. The visitor function code looked like this:

```js
var aqlfunctions = require("org/arangodb/aql/functions");

aqlfunctions.register("myfunctions::leafNodeVisitor", function (config, result, vertex, path, connected) {
  if (connected && connected.length === 0) {
    return vertex.name + " (" + vertex.type + ")";
  }
});
```

And the AQL query to make use of it:

```js
LET params = {
  order: "preorder-expander",
  visitor: "myfunctions::leafNodeVisitor",
  visitorReturnsResults: true
}
FOR result IN GRAPH_TRAVERSAL("worldCountry", "worldVertices/world", "inbound", params)
  RETURN result
```

To traverse the graph starting at vertex `worldVertices/world` using native
AQL traversal and a named graph, we can simply do:

```js
FOR v IN 0..10 INBOUND "worldVertices/world" GRAPH "worldCountry"
  RETURN v
```

It will give us all vertex documents including the start vertex (because the
minimum depth is set to *0*). The maximum depth is set to *10*, which is enough
to follow all edges and reach the leaf nodes in this graph.

The query can be modified to return a formatted path from first to last node:

```js
FOR v, e, p IN 0..10 INBOUND "worldVertices/world" GRAPH "worldCountry"
  RETURN CONCAT_SEPARATOR(" -> ", p.vertices[*].name)
```

The result looks like this (shortened):

```json
[
  "World",
  "World -> Africa",
  "World -> Africa -> Cote d'Ivoire",
  "World -> Africa -> Cote d'Ivoire -> Yamoussoukro",
  "World -> Africa -> Angola",
  "World -> Africa -> Angola -> Luanda",
  "World -> Africa -> Chad",
  "World -> Africa -> Chad -> N'Djamena",
  ...
]
```

As we can see, all possible paths of varying lengths are returned. We are not
really interested in them, but we still have to do the traversal to go from
*World* all the way to the leaf nodes (e.g. *Yamoussoukro*). To determine
if a vertex is really the last on the path in the sense of being a leaf node,
we can use another traversal of depth 1 to check if there is at least one
outgoing edge - which means the vertex is not a leaf node, otherwise it is:

```js
FOR v IN 0..10 INBOUND "worldVertices/world" GRAPH "worldCountry"
  FILTER LENGTH(FOR vv IN INBOUND v GRAPH "worldCountry" LIMIT 1 RETURN 1) == 0
  RETURN CONCAT(v.name, " (", v.type, ")")
```

Using the current vertex `v` as starting point, the second traversal is
performed. It can return early after one edge was followed (`LIMIT 1`),
because we don't need to know the exact count and it is faster this way.
We also don't need the actual vertex, so we can just `RETURN 1` as dummy
value as an optimization. The traversal (which is a sub-query) will
return an empty array in case of a leaf node, and `[ 1 ]` otherwise.
Since we only want the leaf nodes, we `FILTER` out all non-empty arrays
and what is left are the leaf nodes only. The attributes `name` and
`type` are formatted the way they were like in the original JavaScript
code, but now with AQL. The final result is a list of all capitals:

```json
[
  "Yamoussoukro (capital)",
  "Luanda (capital)",
  "N'Djamena (capital)",
  "Algiers (capital)",
  "Yaounde (capital)",
  "Ouagadougou (capital)",
  "Gaborone (capital)",
  "Asmara (capital)",
  "Cairo (capital)",
  ...
]
```

There is no direct substitute for the `GRAPH_TRAVERSAL_TREE()` function.
The advantage of this function was that its (possibly highly nested) result
data structure inherently represented the "longest" possible paths only.
With native AQL traversal, all paths from minimum to maximum traversal depth
are returned, including the "short" paths as well:

```js
FOR v, e, p IN 1..2 INBOUND "worldVertices/continent-north-america" GRAPH "worldCountry"
  RETURN CONCAT_SEPARATOR(" <- ", p.vertices[*]._key)
```

```json
[
  "continent-north-america <- country-antigua-and-barbuda",
  "continent-north-america <- country-antigua-and-barbuda <- capital-saint-john-s",
  "continent-north-america <- country-barbados",
  "continent-north-america <- country-barbados <- capital-bridgetown",
  "continent-north-america <- country-canada",
  "continent-north-america <- country-canada <- capital-ottawa",
  "continent-north-america <- country-bahamas",
  "continent-north-america <- country-bahamas <- capital-nassau"
]
```

A second traversal with depth = 1 can be used to check if we reached a leaf node
(no more incoming edges). Based on this information, the "short" paths can be
filtered out. Note that a second condition is required: it is possible that the
last node in a traversal is not a leaf node if the maximum traversal depth is
exceeded. Thus, we need to also let paths through, which contain as many edges
as hops we do in the traversal (here: 2).

```js
FOR v, e, p IN 1..2 INBOUND "worldVertices/continent-north-america" GRAPH "worldCountry"
  LET other = (
    FOR vv, ee IN INBOUND v GRAPH "worldCountry"
      //FILTER ee != e // needed if traversing edges in ANY direction
      LIMIT 1
      RETURN 1
  )
  FILTER LENGTH(other) == 0 || LENGTH(p.edges) == 2
  RETURN CONCAT_SEPARATOR(" <- ", p.vertices[*]._key)
```

```json
[
  "continent-north-america <- country-antigua-and-barbuda <- capital-saint-john-s",
  "continent-north-america <- country-barbados <- capital-bridgetown",
  "continent-north-america <- country-canada <- capital-ottawa",
  "continent-north-america <- country-bahamas <- capital-nassau"
]
```

The full paths can be returned, but it is not in a tree-like structure as 
with `GRAPH_TRAVERSAL_TREE()`. Such a data structure can be constructed on
client-side if really needed.

```js
FOR v, e, p IN 1..2 INBOUND "worldVertices/continent-north-america" GRAPH "worldCountry"
  LET other = (FOR vv, ee IN INBOUND v GRAPH "worldCountry" LIMIT 1 RETURN 1)
  FILTER LENGTH(other) == 0 || LENGTH(p.edges) == 2
  RETURN p
```

Path data (shortened):

```json
[
  {
    "edges": [
      {
        "_id": "worldEdges/57585025",
        "_from": "worldVertices/country-antigua-and-barbuda",
        "_to": "worldVertices/continent-north-america",
        "type": "is-in"
      },
      {
        "_id": "worldEdges/57585231",
        "_from": "worldVertices/capital-saint-john-s",
        "_to": "worldVertices/country-antigua-and-barbuda",
        "type": "is-in"
      }
    ],
    "vertices": [
      {
        "_id": "worldVertices/continent-north-america",
        "name": "North America",
        "type": "continent"
      },
      {
        "_id": "worldVertices/country-antigua-and-barbuda",
        "code": "ATG",
        "name": "Antigua and Barbuda",
        "type": "country"
      },
      {
        "_id": "worldVertices/capital-saint-john-s",
        "name": "Saint John's",
        "type": "capital"
      }
    ]
  },
  {
    ...
  }
]
```

The first and second vertex of the nth path are connected by the first edge
(`p[n].vertices[0]` ⟝ `p[n].edges[0]` → `p[n].vertices[1]`) and so on. This
structure might actually be more convenient to process compared to a tree-like
structure. Note that the edge documents are also included, in constrast to the
removed graph traversal function.

Contact us via our social channels if you need further help.

**Author:** [Michael Hackstein](https://github.com/mchacki)

**Tags**: #howto #aql #migration
