k Shortest Paths in AQL
=======================

General query idea
--------------------

This type of query is supposed to find the first *k* paths in order of length
(or weight) between two given documents, *startVertex* and *targetVertex* in
your graph.

Every such path will be returned as a JSON object with three components:

- an array containing the `vertices` on the path
- an array containing the `edges` on the path
- the `weight` of the path, that is the sum of all edge weights

If no *weightAttribute* is given, the weight of the path is just its length.

**Example**

Let su take a look at a simple example to explain how it works.
This is the graph that we are going to find some shortest path on:

![train_map](train_map.png)

Each ellipse stands for a train station with the name of the city written inside
of it. They are the vertices of the graph. Arrows represent train connections
between cities and are the edges of the graph. The numbers near the arrows
describe how long it takes to get from one station to another. They are used
as edge weights.

Let us assume that we want to go from **Aberdeen** to **London** by train.

We expect to see the following vertices on *the* shortest path, in this order:

1. Aberdeen
2. Leuchars
3. Edinburgh
4. York
5. London

By the way, the weight of the path is: 1.5 + 1.5 + 3.5 + 1.8 = **8.3**.

Let us look at alternative paths next, for example because we know that the
direct connection between York and London does not operate currently.
An alternative path, which is slightly longer, goes like this:

1. Aberdeen
2. Leuchars
3. Edinburgh
4. York
5. **Carlisle**
6. **Birmingham**
7. London

Its weight is: 1.5 + 1.5 + 3.5 + 2.0 + 1.5 = **10.0**.

Another route goes via Glasgow. There are seven stations on the path as well,
however, it is quicker if we compare the edge weights:

1. Aberdeen
2. Leuchars
3. Edinburgh
4. **Glasgow**
5. Carlisle
6. Birmingham
7. London

The path weight is lower: 1.5 + 1.5 + 1.0 + 1.0 + 2.0 + 1.5 = **8.5**.

Syntax
------

The syntax for k Shortest Paths queries is similar to the one for
[Shortest Path](ShortestPath.md) and there are also two options to either
use a named graph or a set of edge collections. It only emits a path
variable however, whereas SHORTEST_PATH emits a vertex and an edge variable.

{% hint 'warning' %}
It is highly recommended that you use a **LIMIT** statement, as
k Shortest Paths is a potentially expensive operation. On large connected
graphs it can return a large number of paths, or perform an expensive
(but unsuccessful) search for more short paths.
{% endhint %}

### Working with named graphs

```
FOR path
  IN OUTBOUND|INBOUND|ANY K_SHORTEST_PATHS
  startVertex TO targetVertex
  GRAPH graphName
  [OPTIONS options]
  [LIMIT offset, count]
```

- `FOR`: emits the variable **path** which contains one path as an object containing 
   `vertices`, `edges`, and the `weight` of the path.
- `IN` `OUTBOUND|INBOUND|ANY`: defines in which direction
  edges are followed (outgoing, incoming, or both)
- `K_SHORTEST_PATHS`: the keyword to compute k Shortest Paths
- **startVertex** `TO` **targetVertex** (both string|object): the two vertices between
  which the paths will be computed. This can be specified in the form of
  a ID string or in the form of a document with the attribute `_id`. All other
  values will lead to a warning and an empty result. If one of the specified
  documents does not exist, the result is empty as well and there is no warning.
- `GRAPH` **graphName** (string): the name identifying the named graph. Its vertex and
  edge collections will be looked up.
- `OPTIONS` **options** (object, *optional*): used to modify the execution of the
  traversal. Only the following attributes have an effect, all others are ignored:
  - **weightAttribute** (string): a top-level edge attribute that should be used
  to read the edge weight. If the attribute does not exist or is not numeric, the
  *defaultWeight* will be used instead.
  - **defaultWeight** (number): this value will be used as fallback if there is
  no *weightAttribute* in the edge document, or if it's not a number. The default
  is 1.
- `LIMIT` (see [LIMIT operation](../Operations/Limit.html), *optional*):
  the maximal number of paths to return. It is highly recommended to use
  a `LIMIT` for `K_SHORTEST_PATHS`.

### Working with collection sets

```
FOR path 
  IN OUTBOUND|INBOUND|ANY K_SHORTEST_PATHS
  startVertex TO targetVertex
  edgeCollection1, ..., edgeCollectionN
  [OPTIONS options]
  [LIMIT offset, count]
```

Instead of `GRAPH graphName` you can specify a list of edge collections.
The involved vertex collections are determined by the edges of the given
edge collections. 

### Traversing in mixed directions

For k shortest paths with a list of edge collections you can optionally specify the
direction for some of the edge collections. Say for example you have three edge
collections *edges1*, *edges2* and *edges3*, where in *edges2* the direction
has no relevance, but in *edges1* and *edges3* the direction should be taken into
account. In this case you can use *OUTBOUND* as general search direction and *ANY*
specifically for *edges2* as follows:

```
FOR vertex IN OUTBOUND K_SHORTEST_PATHS
  startVertex TO targetVertex
  edges1, ANY edges2, edges3
```

All collections in the list that do not specify their own direction will use the
direction defined after `IN` (here: `OUTBOUND`). This allows to use a different
direction for each collection in your path search.

Examples
--------

We load an example graph to get a named graph that reflects some possible
train connections in Europe and North America.

![train_map](train_map.png)

    @startDocuBlockInline GRAPHKSP_01_create_graph
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHKSP_01_create_graph}
    ~addIgnoreCollection("places");
    ~addIgnoreCollection("connections");
    var examples = require("@arangodb/graph-examples/example-graph.js");
    var graph = examples.loadGraph("kShortestPathsGraph");
    db.places.toArray();
    db.connections.toArray();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHKSP_01_create_graph

Suppose we want to query a route from **Aberdeen** to **London**, and compare
the outputs of SHORTEST_PATH and K_SHORTEST_PATHS with LIMIT 1. Note that while
SHORTEST_PATH and K_SHORTEST_PATH with LIMIT 1 should return a path of the same
length (or weight), they do not need to return the same path.

    @startDocuBlockInline GRAPHKSP_02_Aberdeen_to_London
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHKSP_02_Aberdeen_to_London}
    db._query("FOR v, e IN OUTBOUND SHORTEST_PATH 'places/Aberdeen' TO 'places/London' GRAPH 'kShortestPathsGraph' RETURN [v,e]");
    db._query("FOR p IN OUTBOUND K_SHORTEST_PATHS 'places/Aberdeen' TO 'places/London' GRAPH 'kShortestPathsGraph' LIMIT 1 RETURN p");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHKSP_02_Aberdeen_to_London

Next, we can ask for more than one option for a route:

    @startDocuBlockInline GRAPHKSP_03_Aberdeen_to_London
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHKSP_03_Aberdeen_to_London}
    db._query("FOR p IN OUTBOUND K_SHORTEST_PATHS 'places/Aberdeen' TO 'places/London' GRAPH 'kShortestPathsGraph' LIMIT 3 RETURN p");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHKSP_03_Aberdeen_to_London
    
If we ask for routes that don't exist we get an empty result:

    @startDocuBlockInline GRAPHKSP_04_Aberdeen_to_Toronto
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHKSP_04_Aberdeen_to_Toronto}
    db._query("FOR p IN OUTBOUND K_SHORTEST_PATHS 'places/Aberdeen' TO 'places/Toronto' GRAPH 'kShortestPathsGraph' LIMIT 3 RETURN p");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHKSP_04_Aberdeen_to_Toronto
    
We can use the attribute *travelTime* that connections have as edge weights to
take into account which connections are quicker:

    @startDocuBlockInline GRAPHKSP_05_StAndrews_to_Cologne
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHKSP_05_StAndrews_to_Cologne}
    db._query("FOR p IN OUTBOUND K_SHORTEST_PATHS 'places/StAndrews' TO 'places/Cologne' GRAPH 'kShortestPathsGraph' OPTIONS { 'weightAttribute': 'travelTime', defaultWeight: '15'} LIMIT 3 RETURN p");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHKSP_05_StAndrews_to_Cologne

And finally clean up by removing the named graph:

    @startDocuBlockInline GRAPHKSP_99_drop_graph
    @EXAMPLE_ARANGOSH_OUTPUT{GRAPHKSP_99_drop_graph}
    var examples = require("@arangodb/graph-examples/example-graph.js");
    examples.dropGraph("kShortestPathsGraph");
    ~removeIgnoreCollection("places");
    ~removeIgnoreCollection("connections");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock GRAPHKSP_99_drop_graph
