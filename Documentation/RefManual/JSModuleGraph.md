Module "graph"{#JSModuleGraph}
==============================

@NAVIGATE_JSModuleGraph
@EMBEDTOC{JSModuleGraphTOC}

First Steps with Graphs{#JSModuleGraphIntro}
============================================

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

Graph Constructors and Methods{#JSModuleGraphGraph}
===================================================

The graph module provides basic functions dealing with graph structures.  The
examples assume

@verbinclude graph-setup

@anchor JSModuleGraphGraphConstructor
@copydetails JSF_Graph

@CLEARPAGE
@anchor JSModuleGraphGraphAddEdge
@copydetails JSF_Graph_prototype_addEdge

@CLEARPAGE
@anchor JSModuleGraphGraphAddVertex
@copydetails JSF_Graph_prototype_addVertex

@CLEARPAGE
@anchor JSModuleGraphGraphGetEdges
@copydetails JSF_Graph_prototype_getEdges

@CLEARPAGE
@anchor JSModuleGraphGraphGetVertex
@copydetails JSF_Graph_prototype_getVertex

@CLEARPAGE
@anchor JSModuleGraphGraphGetVertices
@copydetails JSF_Graph_prototype_getVertices

@CLEARPAGE
@anchor JSModuleGraphGraphRemoveVertex
@copydetails JSF_Graph_prototype_removeVertex

@CLEARPAGE
@anchor JSModuleGraphGraphRemoveEdge
@copydetails JSF_Graph_prototype_removeEdge

@CLEARPAGE
@anchor JSModuleGraphGraphDrop
@copydetails JSF_Graph_prototype_drop

@CLEARPAGE
Vertex Methods{#JSModuleGraphVertex}
====================================

@anchor JSModuleGraphVertexAddInEdge
@copydetails JSF_Vertex_prototype_addInEdge

@CLEARPAGE
@anchor JSModuleGraphVertexAddOutEdge
@copydetails JSF_Vertex_prototype_addOutEdge

@CLEARPAGE
@anchor JSModuleGraphVertexEdges
@copydetails JSF_Vertex_prototype_edges

@CLEARPAGE
@anchor JSModuleGraphVertexGetId
@copydetails JSF_Vertex_prototype_getId

@CLEARPAGE
@anchor JSModuleGraphVertexGetInEdges
@copydetails JSF_Vertex_prototype_getInEdges

@CLEARPAGE
@anchor JSModuleGraphVertexGetOutEdges
@copydetails JSF_Vertex_prototype_getOutEdges

@CLEARPAGE
@anchor JSModuleGraphVertexGetEdges
@copydetails JSF_Vertex_prototype_getEdges

@CLEARPAGE
@anchor JSModuleGraphVertexGetProperty
@copydetails JSF_Vertex_prototype_getProperty

@CLEARPAGE
@anchor JSModuleGraphVertexGetPropertyKeys
@copydetails JSF_Vertex_prototype_getPropertyKeys

@CLEARPAGE
@anchor JSModuleGraphVertexProperties
@copydetails JSF_Vertex_prototype_properties

@CLEARPAGE
@anchor JSModuleGraphVertexSetProperty
@copydetails JSF_Vertex_prototype_setProperty

@CLEARPAGE
Edge Methods{#JSModuleGraphEdge}
================================

@anchor JSModuleGraphEdgeGetId
@copydetails JSF_Edge_prototype_getId

@CLEARPAGE
@anchor JSModuleGraphEdgeGetInVertex
@copydetails JSF_Edge_prototype_getInVertex

@CLEARPAGE
@anchor JSModuleGraphEdgeGetLabel
@copydetails JSF_Edge_prototype_getLabel

@CLEARPAGE
@anchor JSModuleGraphEdgeGetOutVertex
@copydetails JSF_Edge_prototype_getOutVertex

@CLEARPAGE
@anchor JSModuleGraphEdgeGetPeerVertex
@copydetails JSF_Edge_prototype_getPeerVertex

@CLEARPAGE
@anchor JSModuleGraphEdgeGetProperty
@copydetails JSF_Edge_prototype_getProperty

@CLEARPAGE
@anchor JSModuleGraphEdgeGetPropertyKeys
@copydetails JSF_Edge_prototype_getPropertyKeys

@CLEARPAGE
@anchor JSModuleGraphEdgeProperties
@copydetails JSF_Edge_prototype_properties

@CLEARPAGE
@anchor JSModuleGraphEdgeSetProperty
@copydetails JSF_Edge_prototype_setProperty
