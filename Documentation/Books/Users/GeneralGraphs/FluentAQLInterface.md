Fluent AQL Interface
====================

This chapter describes a fluent interface to query [your graph](../Graphs/README.md).
The philosophy of this interface is to select a group of starting elements (vertices or edges) at first and from there on explore the graph with your query by selecting connected elements.

As an example you can start with a set of vertices, select their direct neighbors and finally their outgoing edges.

The result of this query will be the set of outgoing edges.
For each part of the query it is possible to further refine the resulting set of elements by giving examples for them.

Examples will explain the API on the [social graph](../Graphs/README.md#the-social-graph):

![Social Example Graph](../Graphs/social_graph.png)

Definition of examples
----------------------

@startDocuBlock JSF_general_graph_example_description

Starting Points
---------------

This section describes the entry points for the fluent interface.
The philosophy of this module is to start with a specific subset of vertices or edges and from there on iterate over the graph.

Therefore you get exactly this two entry points:

* Select a set of edges
* Select a set of vertices

### Edges

@startDocuBlock JSF_general_graph_edges

### Vertices

@startDocuBlock JSF_general_graph_vertices

Working with the query cursor
-----------------------------

The fluent query object handles cursor creation and maintenance for you.
A cursor will be created as soon as you request the first result.
If you are unhappy with the current result and want to refine it further you can execute a further step in the query which cleans up the cursor for you.
In this interface you get the complete functionality available for general AQL cursors directly on your query.
The cursor functionality is described in this section.

### ToArray

@startDocuBlock JSF_general_graph_fluent_aql_toArray

### HasNext

@startDocuBlock JSF_general_graph_fluent_aql_hasNext

### Next

@startDocuBlock JSF_general_graph_fluent_aql_next

### Count

@startDocuBlock JSF_general_graph_fluent_aql_count

Fluent queries
--------------

After the selection of the entry point you can now query your graph in
a fluent way, meaning each of the functions on your query returns the query again.
Hence it is possible to chain arbitrary many executions one after the other.
In this section all available query statements are described.

### Edges

@startDocuBlock JSF_general_graph_fluent_aql_edges

### OutEdges

@startDocuBlock JSF_general_graph_fluent_aql_outEdges

### InEdges

@startDocuBlock JSF_general_graph_fluent_aql_inEdges

### Vertices

@startDocuBlock JSF_general_graph_fluent_aql_vertices

### FromVertices

@startDocuBlock JSF_general_graph_fluent_aql_fromVertices

### ToVertices

@startDocuBlock JSF_general_graph_fluent_aql_toVertices

### Neighbors

@startDocuBlock JSF_general_graph_fluent_aql_neighbors

### Restrict

@startDocuBlock JSF_general_graph_fluent_aql_restrict

### Filter

@startDocuBlock JSF_general_graph_fluent_aql_filter

### Path

@startDocuBlock JSF_general_graph_fluent_aql_path

