# Migrating anonymous graph Functions from 2.8 or earlier to 3.0

## Problem

With the release of 3.0 all GRAPH functions have been dropped from AQL in favor of a more
native integration of graph features into the query language. I have used the old graph
functions and want to upgrade to 3.0.

Graph functions covered in this recipe:

* EDGES
* NEIGHBORS
* PATHS
* TRAVERSAL
* TRAVERSAL_TREE

## Solution

### EDGES

The EDGES can be simply replaced by a call to the AQL traversal.

#### No options

The syntax is slightly different but mapping should be simple:

```
// OLD
[..] FOR e IN EDGES(@@edgeCollection, @startId, 'outbound') RETURN e

// NEW
[..] FOR v, e IN OUTBOUND @startId @@edgeCollection RETURN e
```

#### Using EdgeExamples

Examples have to be transformed into AQL filter statements.
How to do this please read the GRAPH_VERTICES section
in [Migrating GRAPH_* Functions from 2.8 or earlier to 3.0](MigratingGraphFunctionsTo3.html).
Apply these filters on the edge variable `e`.

#### Option incluceVertices

In order to include the vertices you just use the vertex variable v as well:

```
// OLD
[..] FOR e IN EDGES(@@edgeCollection, @startId, 'outbound', null, {includeVertices: true}) RETURN e

// NEW
[..] FOR v, e IN OUTBOUND @startId @@edgeCollection RETURN {edge: e, vertex: v}
```

NOTE: The direction cannot be given as a bindParameter any more it has to be hard-coded in the query.

### NEIGHBORS

The NEIGHBORS is a breadth-first-search on the graph with a global unique check for vertices. So we can replace it by a an AQL traversal with these options.
Due to syntax changes the vertex collection of the start vertex is no longer mandatory to be given.
You may have to adjust bindParameteres for this query.

#### No options

The default options did just return the neighbors `_id` value.

```
// OLD
[..] FOR n IN NEIGHBORS(@@vertexCollection, @@edgeCollection, @startId, 'outbound') RETURN n

// NEW
[..] FOR n IN OUTBOUND @startId @@edgeCollection OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN n._id
```

NOTE: The direction cannot be given as a bindParameter any more it has to be hard-coded in the query.

#### Using edgeExamples

Examples have to be transformed into AQL filter statements.
How to do this please read the GRAPH_VERTICES section
in [Migrating GRAPH_* Functions from 2.8 or earlier to 3.0](MigratingGraphFunctionsTo3.html).
Apply these filters on the edge variable `e` which is the second return variable of the traversal statement.

However this is a bit more complicated as it interferes with the global uniqueness check.
For edgeExamples it is sufficent when any edge pointing to the neighbor matches the filter. Using `{uniqueVertices: 'global'}` first picks any edge randomly. Than it checks against this edge only.
If we know there are no vertex pairs with multiple edges between them we can use the simple variant which is save:

```
// OLD
[..] FOR n IN NEIGHBORS(@@vertexCollection, @@edgeCollection, @startId, 'outbound', {label: 'friend'}) RETURN n

// NEW
[..] FOR n, e IN OUTBOUND @startId @@edgeCollection OPTIONS {bfs: true, uniqueVertices: 'global'}
FILTER e.label == 'friend'
RETURN n._id
```

If there may be multiple edges between the same pair of vertices we have to make the distinct check ourselfes and cannot rely on the traverser doing it correctly for us:

```
// OLD
[..] FOR n IN NEIGHBORS(@@vertexCollection, @@edgeCollection, @startId, 'outbound', {label: 'friend'}) RETURN n

// NEW
[..] FOR n, e IN OUTBOUND @startId @@edgeCollection OPTIONS {bfs: true}
FILTER e.label == 'friend'
RETURN DISTINCT n._id
```

#### Option includeData

If you want to include the data simply return the complete document instead of only the `_id`value.

```
// OLD
[..] FOR n IN NEIGHBORS(@@vertexCollection, @@edgeCollection, @startId, 'outbound', null, {includeData: true}) RETURN n

// NEW
[..] FOR n, e IN OUTBOUND @startId @@edgeCollection OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN n
```

### PATHS

This function computes all paths of the entire edge collection (with a given minDepth and maxDepth) as you can imagine this feature is extremely expensive and should never be used.
However paths can again be replaced by AQL traversal.

#### No options
By default paths of length 0 to 10 are returned. And circles are not followed.

```
// OLD
RETURN PATHS(@@vertexCollection, @@edgeCollection, "outbound")

// NEW
FOR start IN @@vertexCollection
FOR v, e, p IN 0..10 OUTBOUND start @@edgeCollection RETURN {source: start, destination: v, edges: p.edges, vertices: p.vertices}
```

#### followCycles

If this option is set we have to modify the options of the traversal by modifying the `uniqueEdges` property:

```
// OLD
RETURN PATHS(@@vertexCollection, @@edgeCollection, "outbound", {followCycles: true})

// NEW
FOR start IN @@vertexCollection
FOR v, e, p IN 0..10 OUTBOUND start @@edgeCollection OPTIONS {uniqueEdges: 'none'} RETURN {source: start, destination: v, edges: p.edges, vertices: p.vertices}
```

#### minDepth and maxDepth

If this option is set we have to give these parameters directly before the direction.

```
// OLD
RETURN PATHS(@@vertexCollection, @@edgeCollection, "outbound", {minDepth: 2, maxDepth: 5})

// NEW
FOR start IN @@vertexCollection
FOR v, e, p IN 2..5 OUTBOUND start @@edgeCollection
RETURN {source: start, destination: v, edges: p.edges, vertices: p.vertices}
```

### TRAVERSAL and TRAVERSAL_TREE

These have been removed and should be replaced by the
[native AQL traversal](https://docs.arangodb.com/3/Manual/Graphs/Traversals/index.html).
There are many potential solutions using the new syntax, but they largely depend
on what exactly you are trying to achieve and would go beyond the scope of this
cookbook. Here is one example how to do the transition, using the
[world graph](https://docs.arangodb.com/3/Manual/Graphs/index.html#the-world-graph)
as data:

In 2.8, it was possible to use `TRAVERSAL()` together with a custom visitor
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
FOR result IN TRAVERSAL(worldVertices, worldEdges, "worldVertices/world", "inbound", params) 
  RETURN result
```

To traverse the graph starting at vertex `worldVertices/world` using native
AQL traversal and an anonymous graph, we can simply do:

```js
FOR v IN 0..10 INBOUND "worldVertices/world" worldEdges
  RETURN v
```

It will give us all vertex documents including the start vertex (because the
minimum depth is set to *0*). The maximum depth is set to *10*, which is enough
to follow all edges and reach the leaf nodes in this graph.

The query can be modified to return a formatted path from first to last node:

```js
FOR v, e, p IN 0..10 INBOUND "worldVertices/world" e
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
FOR v IN 0..10 INBOUND "worldVertices/world" worldEdges
  FILTER LENGTH(FOR vv IN INBOUND v worldEdges LIMIT 1 RETURN 1) == 0
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

There is no direct substitute for the `TRAVERSAL_TREE()` function.
The advantage of this function was that its (possibly highly nested) result
data structure inherently represented the "longest" possible paths only.
With native AQL traversal, all paths from minimum to maximum traversal depth
are returned, including the "short" paths as well:

```js
FOR v, e, p IN 1..2 INBOUND "worldVertices/continent-north-america" worldEdges
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
FOR v, e, p IN 1..2 INBOUND "worldVertices/continent-north-america" worldEdges
  LET other = (
    FOR vv, ee IN INBOUND v worldEdges
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
with `TRAVERSAL_TREE()`. Such a data structure can be constructed on
client-side if really needed.

```js
FOR v, e, p IN 1..2 INBOUND "worldVertices/continent-north-america" worldEdges
  LET other = (FOR vv, ee IN INBOUND v worldEdges LIMIT 1 RETURN 1)
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
