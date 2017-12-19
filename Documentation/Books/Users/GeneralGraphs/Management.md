Graph Management
================

This chapter describes the javascript interface for [creating and modifying named graphs](../Graphs/README.md).
In order to create a non empty graph the functionality to create edge definitions has to be introduced first:

Edge Definitions
----------------

An edge definition is always a directed relation of a graph. Each graph can have arbitrary many relations defined within the edge definitions array.

### Initialize the list

@startDocuBlock JSF_general_graph_edge_definitions

### Extend the list

@startDocuBlock JSF_general_graph_extend_edge_definitions

#### Relation

@startDocuBlock JSF_general_graph_relation


#### Undirected Relation

**Warning: Deprecated**

This function is deprecated and will be removed soon.
Please use [Relation](../GeneralGraphs/Management.md#relation) instead.

@startDocuBlock JSF_general_graph_undirectedRelation

#### Directed Relation

**Warning: Deprecated**

This function is deprecated and will be removed soon.
Please use [Relation](../GeneralGraphs/Management.md#relation) instead.

@startDocuBlock JSF_general_graph_directedRelation

Create a graph
--------------

After having introduced edge definitions a graph can be created.

@startDocuBlock JSF_general_graph_create

#### Complete Example to create a graph

Example Call:

@startDocuBlock JSF_general_graph_create_graph_example1

alternative call:

@startDocuBlock JSF_general_graph_create_graph_example2

### List available graphs

@startDocuBlock JSF_general_graph_list

### Load a graph

@startDocuBlock JSF_general_graph_graph

### Remove a graph

@startDocuBlock JSF_general_graph_drop

Modify a graph definition during runtime
----------------------------------------

After you have created an graph its definition is not immutable.
You can still add, delete or modify edge definitions and vertex collections.

### Extend the edge definitions

@startDocuBlock JSF_general_graph__extendEdgeDefinitions

### Modify an edge definition

@startDocuBlock JSF_general_graph__editEdgeDefinition

### Delete an edge definition

@startDocuBlock JSF_general_graph__deleteEdgeDefinition

### Extend vertex Collections

Each graph can have an arbitrary amount of vertex collections, which are not part of any edge definition of the graph.
These collections are called orphan collections.
If the graph is extended with an edge definition using one of the orphans,
it will be removed from the set of orphan collection automatically.

#### Add a vertex collection

@startDocuBlock JSF_general_graph__addVertexCollection

#### Get the orphaned collections

@startDocuBlock JSF_general_graph__orphanCollections

#### Remove a vertex collection

@startDocuBlock JSF_general_graph__removeVertexCollection


Maniuplating Vertices
---------------------

### Save a vertex

@startDocuBlock JSF_general_graph_vertex_collection_save

### Replace a vertex

@startDocuBlock JSF_general_graph_vertex_collection_replace

### Update a vertex

@startDocuBlock JSF_general_graph_vertex_collection_update

### Remove a vertex

@startDocuBlock JSF_general_graph_vertex_collection_remove

Manipulating Edges
------------------

### Save a new edge

@startDocuBlock JSF_general_graph_edge_collection_save

### Replace an edge

@startDocuBlock JSF_general_graph_edge_collection_replace

### Update an edge

@startDocuBlock JSF_general_graph_edge_collection_update

### Remove an edge

@startDocuBlock JSF_general_graph_edge_collection_remove

### Connect edges

@startDocuBlock JSF_general_graph_connectingEdges
