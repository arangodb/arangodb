Remove Vertex
=============

Deleting vertices with associated edges is currently not handled via AQL while 
the [graph management interface](../../Manual/Graphs/GeneralGraphs/Management.html#remove-a-vertex)
and the
[REST API for the graph module](../../HTTP/Gharial/Vertices.html#remove-a-vertex)
offer a vertex deletion functionality.
However, as shown in this example based on the
[knows_graph](../../Manual/Graphs/index.html#the-knowsgraph), a query for this 
use case can be created.

![Example Graph](../../Manual/Graphs/knows_graph.png)

When deleting vertex **eve** from the graph, we also want the edges
`eve -> alice` and `eve -> bob` to be removed.
The involved graph and its only edge collection has to be known. In this case it 
is the graph **knows_graph** and the edge collection **knows**.

This query will delete **eve** with its adjacent edges:

    @startDocuBlockInline GRAPHTRAV_removeVertex1
    @EXAMPLE_AQL{GRAPHTRAV_removeVertex1}
    @DATASET{knows_graph}
LET edgeKeys = (FOR v, e IN 1..1 ANY 'persons/eve' GRAPH 'knows_graph' RETURN e._key)
LET r = (FOR key IN edgeKeys REMOVE key IN knows) 
REMOVE 'eve' IN persons
    @END_EXAMPLE_AQL
    @endDocuBlock GRAPHTRAV_removeVertex1

This query executed several actions:
* use a graph traversal of depth 1 to get the `_key` of **eve's** adjacent edges
* remove all of these edges from the `knows` collection
* remove vertex **eve** from the `persons` collection

The following query shows a different design to achieve the same result:

    @startDocuBlockInline GRAPHTRAV_removeVertex2
    @EXAMPLE_AQL{GRAPHTRAV_removeVertex2}
    @DATASET{knows_graph}
LET edgeKeys = (FOR v, e IN 1..1 ANY 'persons/eve' GRAPH 'knows_graph'
            REMOVE e._key IN knows)
REMOVE 'eve' IN persons
    @END_EXAMPLE_AQL
    @endDocuBlock GRAPHTRAV_removeVertex2

**Note**: The query has to be adjusted to match a graph with multiple vertex/edge collections.

For example, the [city graph](../../Manual/Graphs/index.html#the-city-graph) 
contains several vertex collections - `germanCity` and `frenchCity` and several 
edge collections -  `french / german / international Highway`.

![Example Graph2](../../Manual/Graphs/cities_graph.png)

To delete city **Berlin** all edge collections `french / german / international Highway` 
have to be considered. The **REMOVE** operation has to be applied on all edge
collections with `OPTIONS { ignoreErrors: true }`. Not using this option will stop the query
whenever a non existing key should be removed in a collection.

    @startDocuBlockInline GRAPHTRAV_removeVertex3
    @EXAMPLE_AQL{GRAPHTRAV_removeVertex3}
    @DATASET{routeplanner}
LET edgeKeys = (FOR v, e IN 1..1 ANY 'germanCity/Berlin' GRAPH 'routeplanner' RETURN e._key)
LET r = (FOR key IN edgeKeys REMOVE key IN internationalHighway
        OPTIONS { ignoreErrors: true } REMOVE key IN germanHighway
        OPTIONS { ignoreErrors: true } REMOVE key IN frenchHighway
        OPTIONS { ignoreErrors: true }) 
REMOVE 'Berlin' IN germanCity
    @END_EXAMPLE_AQL
    @endDocuBlock GRAPHTRAV_removeVertex3
