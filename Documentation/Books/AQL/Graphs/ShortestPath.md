Shortest Path in AQL
====================

General query idea
------------------

This type of query is supposed to find the shortest path between two given documents
(*startVertex* and *targetVertex*) in your graph. For all vertices on this shortest
path you will get a result in form of a set with two items:

1. The vertex on this path.
2. The edge pointing to it.

### Example execution

Let's take a look at a simple example to explain how it works.
This is the graph that we are going to find a shortest path on:

![traversal graph](traversal_graph.png)

Now we use the following parameters for our query:

1. We start at the vertex **A**.
2. We finish with the vertex **D**.

So obviously we will have the vertices **A**, **B**, **C** and **D** on the
shortest path in exactly this order. Than the shortest path statement will
return the following pairs:

| Vertex | Edge  |
|--------|-------|
|    A   | null  |
|    B   | A → B |
|    C   | B → C |
|    D   | C → D |

Note: The first edge will always be `null` because there is no edge pointing
to the *startVertex*.

Syntax
------

Now let's see how we can write a shortest path query.
You have two options here, you can either use a named graph or a set of edge
collections (anonymous graph).

#### Working with named graphs

```
FOR vertex[, edge]
  IN OUTBOUND|INBOUND|ANY SHORTEST_PATH
  startVertex TO targetVertex
  GRAPH graphName
  [OPTIONS options]
```

- `FOR`: emits up to two variables:
  - **vertex** (object): the current vertex on the shortest path
  - **edge** (object, *optional*): the edge pointing to the vertex
- `IN` `OUTBOUND|INBOUND|ANY`: defines in which direction edges are followed
  (outgoing, incoming, or both)
- **startVertex** `TO` **targetVertex** (both string|object): the two vertices between
  which the shortest path will be computed. This can be specified in the form of
  an ID string or in the form of a document with the attribute `_id`. All other
  values will lead to a warning and an empty result. If one of the specified
  documents does not exist, the result is empty as well and there is no warning.
- `GRAPH` **graphName** (string): the name identifying the named graph. Its vertex and
  edge collections will be looked up.
- `OPTIONS` **options** (object, *optional*): used to modify the execution of the
  traversal. Only the following attributes have an effect, all others are ignored:
  - **weightAttribute** (string): a top-level edge attribute that should be used
  to read the edge weight. If the attribute is not existent or not numeric, the
  *defaultWeight* will be used instead.
  - **defaultWeight** (number): this value will be used as fallback if there is
  no *weightAttribute* in the edge document, or if it's not a number. The default
  is 1.

### Working with collection sets

```
FOR vertex[, edge]
  IN OUTBOUND|INBOUND|ANY SHORTEST_PATH
  startVertex TO targetVertex
  edgeCollection1, ..., edgeCollectionN
  [OPTIONS options]
```

Instead of `GRAPH graphName` you may specify a list of edge collections (anonymous
graph). The involved vertex collections are determined by the edges of the given
edge collections. The rest of the behavior is similar to the named version.

### Traversing in mixed directions

For shortest path with a list of edge collections you can optionally specify the
direction for some of the edge collections. Say for example you have three edge
collections *edges1*, *edges2* and *edges3*, where in *edges2* the direction
has no relevance, but in *edges1* and *edges3* the direction should be taken into
account. In this case you can use *OUTBOUND* as general search direction and *ANY*
specifically for *edges2* as follows:

```
FOR vertex IN OUTBOUND SHORTEST_PATH
  startVertex TO targetVertex
  edges1, ANY edges2, edges3
```

All collections in the list that do not specify their own direction will use the
direction defined after *IN* (here: *OUTBOUND*). This allows to use a different
direction for each collection in your path search.

Conditional shortest path
-------------------------

The SHORTEST_PATH computation will only find an unconditioned shortest path.
With this construct it is not possible to define a condition like: "Find the
shortest path where all edges are of type *X*". If you want to do this, use a
normal [Traversal](Traversals.md) instead with the option `{bfs: true}` in
combination with `LIMIT 1`.

Examples
--------
We will create a simple symmetric traversal demonstration graph:

![traversal graph](traversal_graph.png)

    @startDocuBlockInline GRAPHSP_01_create_graph
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHSP_01_create_graph}
    ~addIgnoreCollection("circles");
    ~addIgnoreCollection("edges");
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var graph = examples.loadGraph("traversalGraph");
    db.circles.toArray();
    db.edges.toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHSP_01_create_graph

We start with the shortest path from **A** to **D** as above:

    @startDocuBlockInline GRAPHSP_02_A_to_D
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHSP_02_A_to_D}
    db._query("FOR v, e IN OUTBOUND SHORTEST_PATH 'circles/A' TO 'circles/D' GRAPH 'traversalGraph' RETURN [v._key, e._key]");
    db._query("FOR v, e IN OUTBOUND SHORTEST_PATH 'circles/A' TO 'circles/D' edges RETURN [v._key, e._key]");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHSP_02_A_to_D

We can see our expectations are fulfilled. We find the vertices in the correct ordering and
the first edge is *null*, because no edge is pointing to the start vertex on t his path.

We can also compute shortest paths based on documents found in collections:

    @startDocuBlockInline GRAPHSP_03_A_to_D
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHSP_03_A_to_D}
    db._query("FOR a IN circles FILTER a._key == 'A' FOR d IN circles FILTER d._key == 'D' FOR v, e IN OUTBOUND SHORTEST_PATH a TO d GRAPH 'traversalGraph' RETURN [v._key, e._key]");
    db._query("FOR a IN circles FILTER a._key == 'A' FOR d IN circles FILTER d._key == 'D' FOR v, e IN OUTBOUND SHORTEST_PATH a TO d edges RETURN [v._key, e._key]");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHSP_03_A_to_D


And finally clean it up again:

    @startDocuBlockInline GRAPHSP_99_drop_graph
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHSP_99_drop_graph}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    examples.dropGraph("traversalGraph");
    ~removeIgnoreCollection("circles");
    ~removeIgnoreCollection("edges");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHSP_99_drop_graph
