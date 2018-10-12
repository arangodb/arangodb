Traversals
==========

ArangoDB provides [several ways to query graph data](../README.md).
Very simple operations can be composed with the low-level edge methods *edges*, *inEdges*, and *outEdges* for
[edge collections](../Edges/README.md). These work on named and anonymous graphs. For more complex operations,
ArangoDB provides predefined traversal objects.

Traversals have also been added to AQL.
Please read the [chapter about AQL traversals](../../../AQL/Graphs/Traversals.html) before you continue reading here.
Most of the traversal cases are covered by AQL and will be executed in an optimized way.
Only if the logic for your is too complex to be defined using AQL filters you can use the traversal object defined
here which gives you complete programmatic access to the data.

For any of the following examples, we'll be using the example collections *v* and *e*,
populated with continents, countries and capitals data listed below (see [Example Data](ExampleData.md)).

To learn more about graphs in ArangoDB take the free
[ArangoDB Graph Course](https://www.arangodb.com/arangodb-graph-course).

Starting from Scratch
---------------------

ArangoDB provides the *edges*, *inEdges*, and *outEdges* methods for edge collections.
These methods can be used to quickly determine if a vertex is connected to other vertices,
and which.
This functionality can be exploited to write very simple graph queries in JavaScript.

For example, to determine which edges are linked to the *world* vertex, we can use *inEdges*:

```js
db.e.inEdges('v/world').forEach(function(edge) { 
  require("@arangodb").print(edge._from, "->", edge.type, "->", edge._to); 
});
```

*inEdges* will give us all ingoing edges for the specified vertex *v/world*. The result
is a JavaScript array that we can iterate over and print the results:

```js
v/continent-africa -> is-in -> v/world
v/continent-south-america -> is-in -> v/world
v/continent-asia -> is-in -> v/world
v/continent-australia -> is-in -> v/world
v/continent-europe -> is-in -> v/world
v/continent-north-america -> is-in -> v/world
```

**Note**: *edges*, *inEdges*, and *outEdges* return an array of edges. If we want to retrieve
the linked vertices, we can use each edges' *_from* and *_to* attributes as follows:

```js
db.e.inEdges('v/world').forEach(function(edge) { 
  require("@arangodb").print(db._document(edge._from).name, "->", edge.type, "->", db._document(edge._to).name); 
});
```

We are using the *document* method from the *db* object to retrieve the connected vertices now.

While this may be sufficient for one-level graph operations, writing a traversal by hand
may become too complex for multi-level traversals.

