!CHAPTER Graph Management

This chapter describes the javascript interface for [creating and modifying named graphs](../Graphs/README.md).
In order to create a non empty graph the functionality to create edge definitions has to be introduced first:

!SECTION Edge Definitions

An edge definition is always a directed relation of a graph. Each graph can have arbitrary many relations defined within the edge definitions array.

!SUBSECTION Initialize the list

@startDocuBlock JSF_general_graph_edge_definitions

!SUBSECTION Extend the list

@startDocuBlock JSF_general_graph_extend_edge_definitions

!SUBSUBSECTION Relation

@startDocuBlock JSF_general_graph_relation


!SUBSUBSECTION Undirected Relation

**Warning: Deprecated**

This function is deprecated and will be removed soon.
Please use [Relation](../GeneralGraphs/Management.md#relation) instead.

@startDocuBlock JSF_general_graph_undirectedRelation

!SUBSUBSECTION Directed Relation

**Warning: Deprecated**

This function is deprecated and will be removed soon.
Please use [Relation](../GeneralGraphs/Management.md#relation) instead.

@startDocuBlock JSF_general_graph_directedRelation

!SECTION Create a graph

After having introduced edge definitions a graph can be created.

@startDocuBlock JSF_general_graph_create

!SUBSUBSECTION Complete Example to create a graph

Example Call:

@startDocuBlock JSF_general_graph_create_graph_example1

alternative call:

@startDocuBlock JSF_general_graph_create_graph_example2

!SUBSECTION List available graphs

@startDocuBlock JSF_general_graph_list

!SUBSECTION Load a graph

@startDocuBlock JSF_general_graph_graph

!SUBSECTION Remove a graph

@startDocuBlock JSF_general_graph_drop

!SECTION Modify a graph definition during runtime

After you have created an graph its definition is not immutable.
You can still add, delete or modify edge definitions and vertex collections.

!SUBSECTION Extend the edge definitions

@startDocuBlock JSF_general_graph__extendEdgeDefinitions

!SUBSECTION Modify an edge definition

@startDocuBlock JSF_general_graph__editEdgeDefinition

!SUBSECTION Delete an edge definition

@startDocuBlock JSF_general_graph__deleteEdgeDefinition

!SUBSECTION Extend vertex Collections

Each graph can have an arbitrary amount of vertex collections, which are not part of any edge definition of the graph.
These collections are called orphan collections.
If the graph is extended with an edge definition using one of the orphans,
it will be removed from the set of orphan collection automatically.

!SUBSUBSECTION Add a vertex collection

@startDocuBlock JSF_general_graph__addVertexCollection

!SUBSUBSECTION Get the orphaned collections

@startDocuBlock JSF_general_graph__orphanCollections

!SUBSUBSECTION Remove a vertex collection

@startDocuBlock JSF_general_graph__removeVertexCollection


!SECTION Maniuplating Vertices

!SUBSECTION Save a vertex

@startDocuBlock JSF_general_graph_vertex_collection_save

!SUBSECTION Replace a vertex

@startDocuBlock JSF_general_graph_vertex_collection_replace

!SUBSECTION Update a vertex

@startDocuBlock JSF_general_graph_vertex_collection_update

!SUBSECTION Remove a vertex

@startDocuBlock JSF_general_graph_vertex_collection_remove

!SECTION Manipulating Edges

!SUBSECTION Save a new edge

@startDocuBlock JSF_general_graph_edge_collection_save

!SUBSECTION Replace an edge

@startDocuBlock JSF_general_graph_edge_collection_replace

!SUBSECTION Update an edge

@startDocuBlock JSF_general_graph_edge_collection_update

!SUBSECTION Remove an edge

@startDocuBlock JSF_general_graph_edge_collection_remove

!SUBSECTION Connect edges

@startDocuBlock JSF_general_graph_connectingEdges
