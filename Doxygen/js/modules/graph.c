////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


void dummy_34 ();

////////////////////////////////////////////////////////////////////////////////
/// @page Graphs First Steps with Graphs
///
/// Graphs consists of vertices and edges. The vertex collection contains the
/// documents forming the vertices. The edge collection contains the documents
/// forming the edges. Together both collections form a graph. Assume that
/// the vertex collection is called @LIT{vertices} and the edges collection
/// @LIT{edges}, then you can build a graph using the @FN{Graph} constructor.
///
/// @verbinclude graph25
///
/// It is possible to use different edges with the same vertices. For
/// instance, to build a new graph with a different edge collection use
///
/// @verbinclude graph26
///
/// It is, however, impossible to use different vertices with the same
/// edges. Edges are tied to the vertices.
////////////////////////////////////////////////////////////////////////////////

void dummy_54 ();

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleGraphTOC
///
/// <ol>
///   <li>Graph</li>
///     <ol>
///       <li>@ref JSModuleGraphGraphAddEdge "Graph.addEdge"</li>
///       <li>@ref JSModuleGraphGraphAddVertex "Graph.addVertex"</li>
///       <li>@ref JSModuleGraphGraphConstructor "Graph constructor"</li>
///       <li>@ref JSModuleGraphGraphGetVertex "Graph.getEdges"</li>
///       <li>@ref JSModuleGraphGraphGetVertex "Graph.getVertex"</li>
///       <li>@ref JSModuleGraphGraphGetVertices "Graph.getVertices"</li>
///       <li>@ref JSModuleGraphGraphRemoveEdge "Graph.removeEdge"</li>
///       <li>@ref JSModuleGraphGraphRemoveVertex "Graph.removeVertex"</li>
///     </ol>
///   <li>Vertex</li>
///     <ol>
///       <li>@ref JSModuleGraphVertexAddInEdge "Vertex.addInEdge"</li>
///       <li>@ref JSModuleGraphVertexAddOutEdge "Vertex.addOutEdge"</li>
///       <li>@ref JSModuleGraphVertexEdges "Vertex.edges"</li>
///       <li>@ref JSModuleGraphVertexGetId "Vertex.getId"</li>
///       <li>@ref JSModuleGraphVertexGetInEdges "Vertex.getInEdges"</li>
///       <li>@ref JSModuleGraphVertexGetOutEdges "Vertex.getOutEdges"</li>
///       <li>@ref JSModuleGraphVertexGetProperty "Vertex.getProperty"</li>
///       <li>@ref JSModuleGraphVertexGetPropertyKeys "Vertex.getPropertyKeys"</li>
///       <li>@ref JSModuleGraphVertexProperties "Vertex.properties"</li>
///       <li>@ref JSModuleGraphVertexSetProperty "Vertex.setProperty"</li>
///     </ol>
///   <li>Edge</li>
///     <ol>
///       <li>@ref JSModuleGraphEdgeGetId "Edge.getId"</li>
///       <li>@ref JSModuleGraphEdgeGetInVertex "Edge.getInVertex"</li>
///       <li>@ref JSModuleGraphEdgeGetLabel "Edge.getLabel"</li>
///       <li>@ref JSModuleGraphEdgeGetOutVertex "Edge.getOutVertex"</li>
///       <li>@ref JSModuleGraphEdgeGetProperty "Edge.getProperty"</li>
///       <li>@ref JSModuleGraphEdgeGetPropertyKeys "Edge.getPropertyKeys"</li>
///       <li>@ref JSModuleGraphEdgeProperties "Edge.properties"</li>
///       <li>@ref JSModuleGraphEdgeSetProperty "Edge.setProperty"</li>
///     </ol>
///   </li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

void dummy_97 ();

////////////////////////////////////////////////////////////////////////////////
/// @page JSModuleGraph Module "graph"
///
/// The graph module provides basic functions dealing with graph structures.
/// It exports the constructors for Graph, Vertex, and Edge.
///
/// <hr>
/// @copydoc JSModuleGraphTOC
/// <hr>
///
/// @anchor JSModuleGraphGraphConstructor
/// @copydetails JSF_Graph
///
/// @anchor JSModuleGraphGraphAddEdge
/// @copydetails JSF_Graph_prototype_addEdge
///
/// @anchor JSModuleGraphGraphAddVertex
/// @copydetails JSF_Graph_prototype_addVertex
///
/// @anchor JSModuleGraphGraphGetEdges
/// @copydetails JSF_Graph_prototype_getEdges
///
/// @anchor JSModuleGraphGraphGetVertex
/// @copydetails JSF_Graph_prototype_getVertex
///
/// @anchor JSModuleGraphGraphGetVertices
/// @copydetails JSF_Graph_prototype_getVertices
///
/// @anchor JSModuleGraphGraphRemoveVertex
/// @copydetails JSF_Graph_prototype_removeVertex
///
/// @anchor JSModuleGraphGraphRemoveEdge
/// @copydetails JSF_Graph_prototype_removeEdge
///
/// <hr>
///
/// @anchor JSModuleGraphVertexAddInEdge
/// @copydetails JSF_Vertex_prototype_addInEdge
///
/// @anchor JSModuleGraphVertexAddOutEdge
/// @copydetails JSF_Vertex_prototype_addOutEdge
///
/// @anchor JSModuleGraphVertexEdges
/// @copydetails JSF_Vertex_prototype_edges
///
/// @anchor JSModuleGraphVertexGetId
/// @copydetails JSF_Vertex_prototype_getId
///
/// @anchor JSModuleGraphVertexGetInEdges
/// @copydetails JSF_Vertex_prototype_getInEdges
///
/// @anchor JSModuleGraphVertexGetOutEdges
/// @copydetails JSF_Vertex_prototype_getOutEdges
///
/// @anchor JSModuleGraphVertexGetProperty
/// @copydetails JSF_Vertex_prototype_getProperty
///
/// @anchor JSModuleGraphVertexGetPropertyKeys
/// @copydetails JSF_Vertex_prototype_getPropertyKeys
///
/// @anchor JSModuleGraphVertexProperties
/// @copydetails JSF_Vertex_prototype_properties
///
/// @anchor JSModuleGraphVertexSetProperty
/// @copydetails JSF_Vertex_prototype_setProperty
///
/// <hr>
///
/// @anchor JSModuleGraphEdgeGetId
/// @copydetails JSF_Edge_prototype_getId
///
/// @anchor JSModuleGraphEdgeGetInVertex
/// @copydetails JSF_Edge_prototype_getInVertex
///
/// @anchor JSModuleGraphEdgeGetLabel
/// @copydetails JSF_Edge_prototype_getLabel
///
/// @anchor JSModuleGraphEdgeGetOutVertex
/// @copydetails JSF_Edge_prototype_getOutVertex
///
/// @anchor JSModuleGraphEdgeGetProperty
/// @copydetails JSF_Edge_prototype_getProperty
///
/// @anchor JSModuleGraphEdgeGetPropertyKeys
/// @copydetails JSF_Edge_prototype_getPropertyKeys
///
/// @anchor JSModuleGraphEdgeProperties
/// @copydetails JSF_Edge_prototype_properties
///
/// @anchor JSModuleGraphEdgeSetProperty
/// @copydetails JSF_Edge_prototype_setProperty
///
////////////////////////////////////////////////////////////////////////////////

void dummy_191 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                              EDGE
// -----------------------------------------------------------------------------

void dummy_195 ();

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

void dummy_199 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_204 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new edge object
////////////////////////////////////////////////////////////////////////////////

void JSF_Edge (int graph, int id) {}




////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_230 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

void dummy_234 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_239 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the identifier of an edge
///
/// @FUN{@FA{edge}.getId()}
///
/// Returns the identifier of the @FA{edge}.
///
/// @verbinclude graph13
////////////////////////////////////////////////////////////////////////////////

void JSF_Edge_prototype_getId (int name) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the to vertex
///
/// @FUN{@FA{edge}.getInVertex()}
///
/// Returns the vertex at the head of the @FA{edge}.
///
/// @verbinclude graph21
////////////////////////////////////////////////////////////////////////////////

void JSF_Edge_prototype_getInVertex (int ) {}


////////////////////////////////////////////////////////////////////////////////
/// @brief label of an edge
///
/// @FUN{@FA{edge}.getLabel()}
///
/// Returns the label of the @FA{edge}.
///
/// @verbinclude graph20
////////////////////////////////////////////////////////////////////////////////

void JSF_Edge_prototype_getLabel (int ) {}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the from vertex
///
/// @FUN{@FA{edge}.getOutVertex()}
///
/// Returns the vertex at the tail of the @FA{edge}.
///
/// @verbinclude graph22
////////////////////////////////////////////////////////////////////////////////

void JSF_Edge_prototype_getOutVertex (int ) {}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns a property of an edge
///
/// @FUN{@FA{edge}.getProperty(@FA{name})}
///
/// Returns the property @FA{name} an @FA{edge}.
///
/// @verbinclude graph12
////////////////////////////////////////////////////////////////////////////////

void JSF_Edge_prototype_getProperty (int name) {}


////////////////////////////////////////////////////////////////////////////////
/// @brief gets all property names of an edge
///
/// @FUN{@FA{edge}.getPropertyKeys()}
///
/// Returns all propety names an @FA{edge}.
///
/// @verbinclude graph32
////////////////////////////////////////////////////////////////////////////////

void JSF_Edge_prototype_getPropertyKeys (int ) {}





////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of an edge
///
/// @FUN{@FA{edge}.setProperty(@FA{name}, @FA{value})}
///
/// Changes or sets the property @FA{name} an @FA{edges} to @FA{value}.
///
/// @verbinclude graph14
////////////////////////////////////////////////////////////////////////////////

void JSF_Edge_prototype_setProperty (int name, int value) {}







////////////////////////////////////////////////////////////////////////////////
/// @brief returns all properties of an edge
///
/// @FUN{@FA{edge}.properties()}
///
/// Returns all properties and their values of an @FA{edge}
///
/// @verbinclude graph11
////////////////////////////////////////////////////////////////////////////////

void JSF_Edge_prototype_properties (int ) {}






////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_426 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

void dummy_430 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_435 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief edge printing
////////////////////////////////////////////////////////////////////////////////


void dummy_448 ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_452 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                            VERTEX
// -----------------------------------------------------------------------------

void dummy_456 ();

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

void dummy_460 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_465 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new vertex object
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex (int graph, int id) {}




////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_486 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

void dummy_490 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_495 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an inbound edge
///
/// @FUN{@FA{vertex}.addInEdge(@FA{peer})}
///
/// Creates a new edge from @FA{peer} to @FA{vertex} and returns the edge
/// object.
///
/// @verbinclude graph33
///
/// @FUN{@FA{vertex}.addInEdge(@FA{peer}, @FA{label})}
///
/// Creates a new edge from @FA{peer} to @FA{vertex} with given label and
/// returns the edge object.
///
/// @verbinclude graph23
///
/// @FUN{@FA{vertex}.addInEdge(@FA{peer}, @FA{label}, @FA{data})}
///
/// Creates a new edge from @FA{peer} to @FA{vertex} with given label and
/// properties defined in @FA{data}. Returns the edge object.
///
/// @verbinclude graph24
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_addInEdge (int out, int label, int data) {}


////////////////////////////////////////////////////////////////////////////////
/// @brief adds an outbound edge
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} and returns the edge
/// object.
///
/// @verbinclude graph34
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer}, @FA{label})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} with given @FA{label} and
/// returns the edge object.
///
/// @verbinclude graph27
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer}, @FA{label}, @FA{data})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} with given @FA{label} and
/// properties defined in @FA{data}. Returns the edge object.
///
/// @verbinclude graph28
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_addOutEdge (int ine, int label, int data) {}


////////////////////////////////////////////////////////////////////////////////
/// @brief inbound and outbound edges
///
/// @FUN{@FA{vertex}.edges()}
///
/// Returns a list of in- or outbound edges of the @FA{vertex}.
///
/// @verbinclude graph15
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_edges (int ) {}





////////////////////////////////////////////////////////////////////////////////
/// @brief returns the identifier of a vertex
///
/// @FUN{@FA{vertex}.getId()}
///
/// Returns the identifier of the @FA{vertex}. If the vertex was deleted, then
/// @CODE{undefined} is returned.
///
/// @verbinclude graph8
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_getId (int name) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges with given label
///
/// @FUN{@FA{vertex}.getInEdges(@FA{label}, ...)}
///
/// Returns a list of inbound edges of the @FA{vertex} with given label(s).
///
/// @verbinclude graph18
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_getInEdges (int ) {}







////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges with given label
///
/// @FUN{@FA{vertex}.getOutEdges(@FA{label}, ...)}
///
/// Returns a list of outbound edges of the @FA{vertex} with given label(s).
///
/// @verbinclude graph19
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_getOutEdges (int ) {}







////////////////////////////////////////////////////////////////////////////////
/// @brief returns a property of a vertex
///
/// @FUN{@FA{vertex}.getProperty(@FA{name})}
///
/// Returns the property @FA{name} a @FA{vertex}.
///
/// @verbinclude graph5
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_getProperty (int name) {}


////////////////////////////////////////////////////////////////////////////////
/// @brief gets all property names of a vertex
///
/// @FUN{@FA{vertex}.getPropertyKeys()}
///
/// Returns all propety names a @FA{vertex}.
///
/// @verbinclude graph7
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_getPropertyKeys (int ) {}





////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges
///
/// @FUN{@FA{vertex}.inbound()}
///
/// Returns a list of inbound edges of the @FA{vertex}.
///
/// @verbinclude graph16
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_inbound (int ) {}





////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges
///
/// @FUN{@FA{vertex}.outbound()}
///
/// Returns a list of outbound edges of the @FA{vertex}.
///
/// @verbinclude graph17
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_outbound (int ) {}





////////////////////////////////////////////////////////////////////////////////
/// @brief returns all properties of a vertex
///
/// @FUN{@FA{vertex}.properties()}
///
/// Returns all properties and their values of a @FA{vertex}
///
/// @verbinclude graph4
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_properties (int ) {}






////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of a vertex
///
/// @FUN{@FA{vertex}.setProperty(@FA{name}, @FA{value})}
///
/// Changes or sets the property @FA{name} a @FA{vertex} to @FA{value}.
///
/// @verbinclude graph6
////////////////////////////////////////////////////////////////////////////////

void JSF_Vertex_prototype_setProperty (int name, int value) {}








////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_870 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

void dummy_874 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_879 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex representation
////////////////////////////////////////////////////////////////////////////////


void dummy_892 ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_896 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                             GRAPH
// -----------------------------------------------------------------------------

void dummy_900 ();

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

void dummy_904 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_909 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new graph object
///
/// @FUN{Graph(@FA{vertices}, @FA{edges})}
///
/// Constructs a new graph object using the collection @FA{vertices} for all
/// vertices and the collection @FA{edges} for all edges. Note that it is
/// possible to construct two graphs with the same vertex set, but different
/// edge sets.
///
/// @verbinclude graph1
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph (int vertices, int edg) {}





////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_947 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

void dummy_951 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_956 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an edge to the graph
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in})}
///
/// Creates a new edge from @FA{out} to @FA{in} and returns the edge
/// object.
///
/// @verbinclude graph30
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{label})}
///
/// Creates a new edge from @FA{out} to @FA{in} with @FA{label} and returns the
/// edge object.
///
/// @verbinclude graph9
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{data})}
///
/// Creates a new edge and returns the edge object. The edge contains the
/// properties defined in @FA{data}.
///
/// @verbinclude graph31
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{label}, @FA{data})}
///
/// Creates a new edge and returns the edge object. The edge has the
/// label @FA{label} and contains the properties defined in @FA{data}.
///
/// @verbinclude graph10
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph_prototype_addEdge (int out, int ine, int label, int data) {}








////////////////////////////////////////////////////////////////////////////////
/// @brief adds a vertex to the graph
///
/// @FUN{@FA{graph}.addVertex()}
///
/// Creates a new vertex and returns the vertex object.
///
/// @verbinclude graph2
///
/// @FUN{@FA{graph}.addVertex(@FA{data})}
///
/// Creates a new vertex and returns the vertex object. The vertex contains
/// the properties defined in @FA{data}.
///
/// @verbinclude graph3
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph_prototype_addVertex (int data) {}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns a vertex given its id
///
/// @FUN{@FA{graph}.getVertex(@FA{id})}
///
/// Returns the vertex identified by @FA{id} or undefined.
///
/// @verbinclude graph29
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph_prototype_getVertex (int id) {}



////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all vertices
///
/// @FUN{@FA{graph}.getVertices()}
///
/// Returns an iterator for all vertices of the graph. The iterator supports the
/// methods @FN{hasNext} and @FN{next}.
///
/// @verbinclude graph35
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph_prototype_getVertices (int ) {}


void JSF_iterator (int ) {}







////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all edges
///
/// @FUN{@FA{graph}.getEdges()}
///
/// Returns an iterator for all edges of the graph. The iterator supports the
/// methods @FN{hasNext} and @FN{next}.
///
/// @verbinclude graph36
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph_prototype_getEdges (int ) {}


void JSF_iterator (int ) {}







////////////////////////////////////////////////////////////////////////////////
/// @brief removes a vertex and all in- or out-bound edges
///
/// @FUN{@FA{graph}.removeVertex(@FA{vertex})}
///
/// Deletes the @FA{vertex} and all its edges.
///
/// @verbinclude graph37
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph_prototype_removeVertex (int vertex) {}






////////////////////////////////////////////////////////////////////////////////
/// @brief removes an edge
///
/// @FUN{@FA{graph}.removeEdge(@FA{vertex})}
///
/// Deletes the @FA{edge}. Note that the in and out vertices are left untouched.
///
/// @verbinclude graph38
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph_prototype_removeEdge (int edge) {}





////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_1223 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

void dummy_1227 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

void dummy_1232 ();

////////////////////////////////////////////////////////////////////////////////
/// @brief private function to construct a vertex
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph_prototype_constructVertex (int id) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief private function to construct an edge
////////////////////////////////////////////////////////////////////////////////

void JSF_Graph_prototype_constructEdge (int id) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief graph printing
////////////////////////////////////////////////////////////////////////////////


void dummy_1256 ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_1260 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

void dummy_1264 ();

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////


void dummy_1275 ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

void dummy_1279 ();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
