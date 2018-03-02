Graph operations
================

This chapter describes graph related AQL functions.

### Short explanation of the example parameter

A lot of the following functions accept a vertex (or edge) example as parameter. This can contain the following: 

* {}                : Returns all possible vertices for this graph
* *idString*        : Returns the vertex/edge with the id *idString*
* [*idString1*, *idString2* ...] : Returns the vertices/edges with the ids matching the given strings. 
* {*key1* : *value1*, *key2* : *value2*} : Returns the vertices/edges that match this example, which means that both have *key1* and *key2* with the corresponding attributes
* {*key1.key2* : *value1*, *key3* : *value2*} : It is possible to chain keys, which means that a document *{key1 : {key2 : value1}, key3 : value2}* would be a match
* [{*key1* : *value1*}, {*key2* : *value2*}] : Returns the vertices/edges that match one of the examples, which means that either *key1* or *key2* are set with the corresponding value

### The complexity of the shortest path algorithms

Most of the functions described in this chapter calculate the shortest paths for subsets of the graphs vertices.
Hence the complexity of these functions depends of the chosen algorithm for this task. For
[Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) it is **O(n^3)**
with *n* being the amount of vertices in the graph. For
[Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm) it would be **O(x\*y\*n^2)** with *n* being
the amount of vertices in the graph, *x* the amount of start vertices and *y* the amount of
target vertices. Hence a suggestion may be to use Dijkstra when x\*y < n and the functions supports choosing your algorithm.

### Example Graph
All examples in this chapter will use [this simple city graph](../Graphs/README.md#the-city-graph):

![Cities Example Graph](../Graphs/cities_graph.png)

### Edges and Vertices related functions

This section describes various AQL functions which can be used to receive information about the graph's vertices, edges, neighbor relationship and shared properties.

### GRAPH_EDGES
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_edges

### GRAPH_VERTICES
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_vertices

### GRAPH_NEIGHBORS
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_neighbors

### GRAPH_COMMON_NEIGHBORS
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_common_neighbors

### GRAPH_COMMON_PROPERTIES
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_common_properties

### Shortest Paths, distances and traversals.
<!-- js/server/modules/org/arangodb/aql.js -->

This section describes AQL functions, that calculate paths from a subset of vertices in a graph to another subset of vertices.

### GRAPH_PATHS
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_paths

### GRAPH_SHORTEST_PATH
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_shortest_paths

### GRAPH_TRAVERSAL
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_traversal

### GRAPH_TRAVERSAL_TREE
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_traversal_tree

### GRAPH_DISTANCE_TO
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_distance

### Graph measurements.
<!-- js/server/modules/org/arangodb/aql.js -->

This section describes AQL functions to calculate various graph related measurements as defined in the mathematical graph theory.

### GRAPH_ABSOLUTE_ECCENTRICITY
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_absolute_eccentricity

### GRAPH_ECCENTRICITY
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_eccentricity

### GRAPH_ABSOLUTE_CLOSENESS
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_absolute_closeness

### GRAPH_CLOSENESS
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_closeness

### GRAPH_ABSOLUTE_BETWEENNESS
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_absolute_betweenness

### GRAPH_BETWEENNESS
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_betweenness

### GRAPH_RADIUS
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_radius

### GRAPH_DIAMETER
<!-- js/server/modules/org/arangodb/aql.js -->

@startDocuBlock JSF_aql_general_graph_diameter

