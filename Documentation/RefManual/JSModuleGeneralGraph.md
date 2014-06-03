Module "general-graph"{#JSModuleGeneralGraph}
==============================

@NAVIGATE_JSModuleGeneralGraph
@EMBEDTOC{JSModuleGeneralGraphTOC}

First Steps with Graphs{#JSModuleGeneralGraphIntro}
===================================================

A Graph consists of vertices and edges. The vertex collection contains the
documents forming the vertices. The edge collection contains the documents
forming the edges. Together both collections form a graph. Assume that the
vertex collection is called `vertices` and the edges collection `edges`, then
you can build a graph using the @FN{Graph} constructor.

@verbinclude graph25

It is possible to use different edges with the same vertices. For instance, to
build a new graph with a different edge collection use

@verbinclude graph26

It is, however, impossible to use different vertices with the same edges. Edges
are tied to the vertices.

Graph Constructors and Methods{#JSModuleGeneralGraphGraph}
==========================================================

The graph module provides basic functions dealing with graph structures.

@verbinclude graph-setup

@anchor JSModuleGraphGraphConstructor
@copydetails JSF_Graph_prototype_initialize

- - -

## Create a graph

The creation of a graph requires the name of the graph and a definition of its edges.

For every type of edge definition a convenience method exists that shall be used to create a graph.

```js
> var graph = require("org/arangodb/graph");

> var g = graph._create(graphName, edgeDefinitions);
```

There are different types of edge defintions:

### Edge Definitions

The edge definitions for a graph is an Array containing arbitrary many directed and/or undirected relations as defined below.
The edge definitons array is initialised with the following call:

```js
> var edgeDefinitions = graph._edgeDefinitions(edgeDefinition1,......edgeDefinitionN);
```

To add further edge definitions to the array one must call:

```js
> graph._extendEdgeDefinitions(edgeDefinitions, edgeDefinition1,......edgeDefinitionN);
```


#### Undirected Relation

@copydetails JSF_general_graph_undirectedRelationDefinition

```js
graph._undirectedRelationDefinition(relationName, vertexCollections);
```

Define an undirected relation.
The *relationName* defines the name of this relation and references to the underlying edge collection.
The *vertexCollections* is an Array of document collections holding the vertices.
Relations are allowed in both directions among all collections in *vertexCollections*.

Example Call:
```js
> graph._undirectedRelationDefinition("eats", ["Human", "Animal"]);
{
  collection: "eats",
  from: ["Human", "Animal"],
  to: ["Human", "Animal"]
}
```

#### Directed Relation

```js
graph._directedRelationDefinition(relationName, fromVertexCollections, toVertexCollections);
```

Define a directed relation.
The *relationName* defines the name of this relation and references to the underlying edge collection.
The *fromVertexCollections* is an Array of document collections holding the start vertices.
The *toVertexCollections* is an Array of document collections holding the target vertices.
Relations are only allowed in the direction from any collection in *fromVertexCollections* to any collection in *toVertexCollections*.

Example Call:
```js
> graph._directedRelationDefinition("has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]);
{
  collection: "has_bought",
  from: ["Customer", "Company"],
  to: ["Groceries", "Electronics"]
}
```


#### Complete Example to create a graph

Example Call:

```js
> var graph = require("org/arangodb/graph");
> var edgeDefinitions = graph._edgeDefinitions();
> graph._extendEdgeDefinitions(edgeDefinitions, graph._undirectedRelationDefinition("friend_of", ["Customer"]));
> graph._extendEdgeDefinitions(edgeDefinitions, graph._directedRelationDefinition("has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]));
> graph._create("myStore", edgeDefinitions);
{
  _id: "_graphs/123",
  _rev: "123",
  _key: "123"
}
```

alternative call:

```js
> var graph = require("org/arangodb/graph");
> var edgeDefinitions = graph._edgeDefinitions(graph._undirectedRelationDefinition("friend_of", ["Customer"]), graph._directedRelationDefinition("has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]));
> graph._create("myStore", edgeDefinitions);
{
  _id: "_graphs/123",
  _rev: "123",
  _key: "123"
};
```


## Read a graph

```js
> var graph = require("org/arangodb/graph");

> var g = graph._graph("myStore");
```

- - -

Remove a graph
--------------

Removes a graph from the collection *\_graphs*.

```js
> graph._drop(graphId, dropCollections);
true
```

graphId: string - id of the graph to be removed
dropCollections: bool - optional. *true* all collections of the graph will be deleted.
*false* no collection of the graph will be deleted. Default: *true*

- - -

## Vertex
### Save

```javascript
graph.<vertexCollectionName>.save(data)
```

Creates and saves a new vertex in collection *vertexCollectionName*

data: json - data of vertex

Example (given graph *g*, collection *v*):
```javascript
> g.v.save({first_name: "Tom"});
{
  "_id" : "v/123",
  "_key" : "123",
  "_rev" : "3175486"
}
```

- - -

### Replace

```javascript
graph.<vertexCollectionName>.replace(vertexId, data, overwrite)
```
Replaces the data of a vertex in collection *vertexCollectionName*

vertexId: string - id of vertex to be replaced  
data: json - data of vertex  
overwrite: bool - controles version conflicts. Default: false  

Example:
```javascript
> g.v.replace("v/123", {name: "Tim"});
{
  "_id" : "v/123",
  "_key" : "123",
  "_rev" : "3175490",
  "_oldRev" : "3175486"
}
```

- - -

### Update

```javascript
graph.<vertexCollectionName>.update(vertexId, data, overwrite, keepNull)
```
Updates the data of a vertex in collection *vertexCollectionName*

vertexId: string - id of vertex to be updated  
data: json - data of vertex  
options: json - (optional) - see collection documentation

Example:
```javascript
> g.v.update("v/123", {age: 42});
{
  "_id" : "v/123",
  "_key" : "123",
  "_rev" : "4034116",
  "_oldRev" : "3175490"
}
```

If there is a conflict, i. e. if the revision of the vertex does not match the revision in the collection, then an error is thrown.  
As before, but in case of a conflict, the conflict is ignored and the old document is overwritten.

Example:
```javascript
> g.v.update("v/123", {age: 42}, {overwrite: true});
{
  "_id" : "v/123",
  "_key" : "123",
  "_rev" : "4034116",
  "_oldRev" : "3175490"
}
```

- - -

### Remove


```javascript
graph.<vertexCollectionName>.remove(vertexId, options)
```
Removes a vertex in collection *vertexCollectionName*

Additionally removes all ingoing and outgoing edges of the vertex recursively
(see [edge remove](#edge.remove)).

vertexId: string - id of vertex to be removed  
options: json - (optional) - see collection documentation

Example:
```javascript
> g.v.remove("v/123");
true
```

- - -


## Edge
### Save


```javascript
graph.<edgeCollectionName>.save(from, to, data)
```

Creates and saves a new edge from vertex *from* to vertex *to* in collection *edgeCollectionName*

from: string - id of outgoing vertex  
to: string -  of ingoing vertex  
data: json - data of edge

Example (given graph *g*, vertexCollection *v*, edgeCollection *e*) where *e* contains *v* in *from* and *to* configuration:
```javascript
> g.v.save({name: "Tim"});
{
  "_id" : "v/123",
  "_key" : "123",
  "_rev" : "3175486"
}
> g.v.save({name: "Tom"});
{
  "_id" : "v/456",
  "_key" : "456",
  "_rev" : "3175487"
}
> g.e.save("v/123", "v/456", {label: "is_friend_of", since: "monday"})
{
  "_id" : "e/789"
  "_key" : "789",
  "_rev" : "3175488"
}
```

Example (given graph *g*, vertexCollection *v*, edgeCollection *e*) where *e* allows vertices from *v* as **start**, but **not** **target** vertex.

```javascript
> g.v.save({name: "Tim"});
{
  "_id" : "v/123",
  "_key" : "123",
  "_rev" : "3175486"
}
> g.v.save({name: "Tom"});
{
  "_id" : "v/456",
  "_key" : "456",
  "_rev" : "3175487"
}
> g.e.save("v/123", "v/456", {label: "is_friend_of", since: "monday"})
[ArangoError XXXX: precondition failed: cannot create edge to v in e]
```

Example (given graph *g*, vertexCollection *v*, edgeCollection *e*) where *e* allows vertices from *v* as **target**, but **not** **start** vertex.

```javascript
> g.v.save({name: "Tim"});
{
  "_id" : "v/123",
  "_key" : "123",
  "_rev" : "3175486"
}
> g.v.save({name: "Tom"});
{
  "_id" : "v/456",
  "_key" : "456",
  "_rev" : "3175487"
}
> g.e.save("v/123", "v/456", {label: "is_friend_of", since: "monday"})
[ArangoError XXXX: precondition failed: cannot create edge from v in e]
```

- - -

### Replace

```javascript
graph.<edgeCollectionName>.replace(id, data, options)
```
Replaces the data of an edge in collection *edgeCollectionName*

id: string - id edge  
data: json - data of edge  
options: json - (optional) - see collection documentation

Example:
```javascript
> g.e.replace("e/789", {label: "knows", status: "friends"})
{
  "_id" : "e/789"
  "_key" : "789",
  "_rev" : "3175489"
  "_oldRev" : "3175488"
}
```

- - -

### Update

```javascript
graph.<edgeCollectionName>.update(id, data, options)
```
Updates the data of an edge in collection *edgeCollectionName*

id: string - id edge  
data: json - data of edge
options: json - (optional) - see collection documentation

Example:
```javascript
> g.e.update("e/789", {since: 2014})
{
  "_id" : "e/789"
  "_key" : "789",
  "_rev" : "3175490"
  "_oldRev" : "3175489"
}
```

- - -

### <a name="edge.remove"></a>Remove

```javascript
graph.<edgeCollectionName>.remove(id, options)
```
Removes an edge in collection *edgeCollectionName*
If this edge is used as a vertex by another edge, the other edge will be removed (recursively).

id: string - id edge  
options: json - (optional) - see collection documentation

Example:
```javascript
> g.e.remove("e/789")
true
```

- - -

Fluent AQL Interface (#JSModuleGeneralGraphFluent)
==================================================

## Starting Points
**To be corrected**
### Edges

#### All edges

```javascript
graph.edges(vertexId)
```
Returns all edges of a vertex

- - -

#### All ingoing edges

```javascript
graph.inEdges(vertexId)
```
Returns all ingoing edges of a vertex

- - -

#### All outgoing edges

```javascript
graph.outEdges(vertexId)
```
Returns all outgoing edges of a vertex

- - -

### Vertices

#### Get ingoing vertex

```javascript
graph.getInVertex(edgeId)
```
Returns the ingoing vertex of an edge

- - -

#### Get outgoing vertex

```javascript
graph.getOutVertex(edgeId)
```
Returns the outgoing vertex of an edge

- - -

## Fluent query options

### Edges

@copydetails JSF_general_graph_fluent_aql_edges

### OutEdges

@copydetails JSF_general_graph_fluent_aql_outEdges

### InEdges

@copydetails JSF_general_graph_fluent_aql_inEdges

### Vertices

@copydetails JSF_general_graph_fluent_aql_vertices

### FromVertices

@copydetails JSF_general_graph_fluent_aql_fromVertices

### ToVertices

@copydetails JSF_general_graph_fluent_aql_toVertices

## Restrict \& Filter
### Restrict

Use *restrict()* to restrict the search to one or more collections.
These collections have to be known inside the graph.

```javascript
graph.edges(id).restrict([edgeCollectionName])
```
```javascript
graph.edges(id).restrict([edgeCollectionName1, edgeCollectionName2])
```

Example Calls:

```javascript
> g.edges("v/123")
[
  {
    _id: "e1/987",
    _key: "987",
    _rev: "987",
    _from: "v/123",
    _to: "v/456"
  },
  {
    _id: "e2/654",
    _key: "654",
    _rev: "654",
    _from: "v/789",
    _to: "v/123"
  }
]
```
```javascript
> g.edges("v/123").restrict(["e1"])
[
  {
    _id: "e1/987",
    _key: "987",
    _rev: "987",
    _from: "v/123",
    _to: "v/456"
  }
]
```
```javascript
> g.edges("v/123").restrict(["e1", "e2"])
[
  {
    _id: "e1/987",
    _key: "987",
    _rev: "987",
    _from: "v/123",
    _to: "v/456"
  },
  {
    _id: "e2/654",
    _key: "654",
    _rev: "654",
    _from: "v/789",
    _to: "v/123"
  }
]
```
```javascript
> g.edges("v/123").restrict(["e1", "unkown"])
[ArangoError XXXX: precondition failed: edge collection unkown is not known to graph]
```

- - -

### Filter

Use *filter()* to provide examples for the search.
```javascript
> g.edges("myCol/myId")
   .filter({
     since: 2010,
     label: 'knows'
    })
   .edges()
```

*since* and *label* are attributes of the edge.
Only equality checks are possible.

- - -

It is possible to combine *filter()*, *let()* and *restrict()* to build more complex requests.

Example:
```javascript
> g.outEdges("myCol/myId")
   .restrict("myCol")
   .filter({
     since: 2010,
     label: 'knows'
   })
   .outVertices()
   .filter({
     name: "Alice"
   })
   .outEdges()
   .filter({
     status: 'friend'
   })
```


Filter \& Let (Old version, broken interface)

Use *filter()* to provide rules to the search.
```javascript
> g.edges("myCol/myId")
   .filter("e.since > 2010,e.label = 'knows'")
   .edges()
```

*e.since* and *e.label* are attributes of the edge.

- - -

Use *let()* to define a variable for later references in *filter()*
```javascript
> g.edges("myCol/myId")
   .let("myLabel = e.label")
   .filter("e.since > 2010 and e.label = myLabel")
```

*e.since* and *e.label* are attributes of the edge.


- - -

It is possible to combine *filter()*, *let()* and *restrict()* to build more complex requests.

Example:
```javascript
> g.outEdges("myCol/myId")
   .restrict("myCol")
   .filter("e.since > 2010 and e.label = 'knows'")
   .outVertices()
   .outEdges()
   .filter("e.status = 'friend'")
```

With *let()* it is possible to assign values to variables within a filter rule to use them in other filters.

Example:
```javascript
> g.outEdges("myCol/myId")
   .restrict("myCol")
   .filter("e.since > 2010")
   .let("labelKnows = e.label")
   .outVertices()
   .filter("v.name = 'Tim'")
   .outEdges()
   .filter("e.status = 'friend' and e.label = labelKnows")
```


Some Methods
------------

```javascript
graph.listCommonNeighbors(vertex1, vertex2, options)
```

vertex1: string - vertex id  
vertex2: string - vertex id  
options:
* see getNeighbors

```javascript
graph.amountCommonNeighbors(vertex1, vertex2, options)
```

vertex1: string - vertex id  
vertex2: string - vertex id  
options:
* see getNeighbors



```javascript
graph.listCommonProperties((vertex1, vertex2)
```

vertex1: string - vertex id  
vertex2: string - vertex id  


```javascript
graph.amountCommonProperties((vertex1, vertex2)
```

vertex1: string - vertex id  
vertex2: string - vertex id  




```javascript
graph.pathTo(vertex1, vertex2, options)
```


vertex1: string - vertex id  
vertex2: string - vertex id  
options:  see determinePredecessors


```javascript
graph.distanceTo(vertex1, vertex2, options)
```


vertex1: string - vertex id  
vertex2: string - vertex id  
options: see determinePredecessors


```javascript
graph.determinePredecessors(vertex1, source, options)
```


vertex1: string - vertex id  
source: ???
options:
* cached: Boolean -> If true a cached version will be used


```javascript
graph.pathesForTree(vertex1, tree, path_to_here)
```

vertex1: string - vertex id  
tree: ???   
path_to_here: Internal Array, should initially be undefined or an empty array


```javascript
graph.getNeighbors(vertex1, options)
```

vertex1: string - vertex id  
options:

* direction:  
  "inbound" -> consider only inbound edges  
  "outbount" -> consider only outbound edges  
  "any"(default) -> consider both directions  
* weight: attribute-name -> use this attribute to determine edgeweight
* weight_function: function -> use this function to calculate the weight
* default-weight -> us this value if weight could not be calculated otherwise, default is Infinity
* only: function -> will be invoked on any edge, neighbors will only be included if this returns true or is not defined. 


```javascript
graph.measurement(vertex1, measurement)
```

vertex1: string - vertex id  
measurement: String
*  "eccentricity": Calculates the eccentricity of the vertex
*  "betweenness": Calculates the betweenness of the vertex
*  "closeness": Calculates the closeness of the vertex

Using Graphs in AQL {#JSModuleGeneralGraphAQL}
=======================================

Complete Documentation can be copied from normal AQL documentation, with:

* replace VertexCollection/EdgeCollection by Graph

PATHS
-----

* **BUILD ON** `ahuacatl.js`: 4090 `GRAPH_PATHS` -> uses `COLLECTION` on second arg, has to use `COLLECTION` or `GRAPH` accordingly. Has to pass the graph to traverser

Paths returns a handle for all paths included in the graph:

`GRAPH_PATHS(graphname, direction, followcycles)`

* `graphname` defines the graph
* `direction` defines the direction
* `followcycles` defines if cyclic paths should be followed

Example calls:

```javascript
FOR p in PATHS(shop, "outbound")
  FILTER p.source._id == "123456/123456" && LENGTH(p.edges) == 2
  RETURN p.vertices[*].name
```


TRAVERSAL
---------

`GRAPH_TRAVERSAL(graphname, startVertex, direction, options)}`

* **BUILD ON** `ahuacatl.js`: 4243 `TRAVERSAL_FUNC` -> uses `COLLECTION` on first and second arg, has to use `COLLECTION` or `GRAPH` accordingly. Has to pass the graph to traverser
* **TO CHANGE** `common/modules/org/arangodb/graph/traversal.js`: 106 `collectionDatasourceFactory` should be able to work on Graphs

Traverses the graph described by the `graphname`, 
starting at the vertex identified by id `startVertex`. Vertex connectivity is
specified by the `direction` parameter:

- `"outbound"`: Vertices are connected in `_from` to `_to` order
- `"inbound"`: Vertices are connected in `_to` to `_from` order
- `"any"`: Vertices are connected in both `_to` to `_from` and in 
  `_from` to `_to` order

All this is defined already for TRAVERSAL, no changes should be applied here

```javascript
TRAVERSAL(shop, "products/arangodb", "outbound", {
  strategy: "depthfirst",
  order: "postorder",
  itemOrder: "backward",
  maxDepth: 6,
  paths: true
})
```


TRAVERSAL_TREES
---------------

`GRAPH_TRAVERSAL_TREE(graphname, startVertex, direction, connectName, options)`

* **BUILD ON** `ahuacatl.js`: 4243 `TRAVERSAL_FUNC` -> uses `COLLECTION` on first and second arg, has to use `COLLECTION` or `GRAPH` accordingly. Has to pass the graph to traverser
* **TO CHANGE** `common/modules/org/arangodb/graph/traversal.js`: 106 `collectionDatasourceFactory` should be able to work on Graphs

```javascript
GRAPH_TRAVERSAL_TREE(shop, "products/arangodb", "inbound", "sold", { 
  itemOrder: "forward"
})
```

Makes internal use of TRAVERSAL, modyfing that is sufficient.

SHORTEST_PATHS
--------------

* **BUILD ON** `ahuacatl.js`: 4243 `TRAVERSAL_FUNC` -> uses `COLLECTION` on first and second arg, has to use `COLLECTION` or `GRAPH` accordingly. Has to pass the graph to traverser
* **TO CHANGE** `common/modules/org/arangodb/graph/traversal.js`: 106 `collectionDatasourceFactory` should be able to work on Graphs

`GRAPH_SHORTEST_PATH(graphname, startVertex, endVertex, direction, options)`:

Equal to functionality of `SHORTEST_PATH`.
Makes internal use of TRAVERSAL, modyfing that is sufficient.


EDGES
-----

* **BUILD ON** `ahuacatl.js`: 4479 `GRAPH_EDGES` -> uses `COLLECTION` on first argument, has to use `COLLECTION` or `GRAPH` accordingly.

`GRAPH_EDGES(graphname, startvertex, direction, edgeexamples, collectionRestrictions)`

Same as original, but with optional `collectionRestrictions`to define which edge collections have to be included. Default is all.


NEIGHBORS
---------

* **BUILD ON** `ahuacatl.js`: 4508 `GRAPH_NEIGHBORS` -> uses `COLLECTION` on first, has to use `COLLECTION` or `GRAPH` accordingly.

`GRAPH_NEIGHBORS(graphname, startvertex, direction, edgeexamples)`

* Each of the graph functions in AQL (`PATHS`, `TRAVERSAL`, `TRAVERSAL_TREES`, `SHORTEST_PATHS`, `EDGES`, `NEIGHBORS`) will take the graph as its first argument (which parts of the other arguments will be pushed to be defined in FILTER and not in the signature of the function was discussed, but postponed because it is a detail).

UNDER THE HOOD
==============

`COLLECTION` in `ahuactl.js`: 247 has to be adapted/mirrored such that it can act on graphs and points to the graph function instead of the collection function.
The graph acts to the outside as one big collection, so internal function calls are equal when called on edge collections and on graphs.
It only has to be figured out if a graph or a collection has to be referenced.

Required functions on graph
---------------------------

Only:

* `outEdges(vertexID)`
* `inEdges(vertexID)`
* `edges(vertexID)`


DREAMCODE (Future wishes)
=========================

Restrict queries of:
 
* `outEdges(vertexID)`
* `inEdges(vertexID)`
* `edges(vertexID)`

To specific collections within AQL.
In graph module this is possible.

* On graph object: `g.cipher(query)`
* AQL: `CIPHER(graphname, query)`

This should execute the cipher `query` on the given graph structure 

**TODO:** Research for all cipher commands, how to map them to AQL/JS

