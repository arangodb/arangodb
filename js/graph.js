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

var internal = require("internal");

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
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page GraphFuncIndexTOC
///
/// <ol>
///   <li>Graph</li>
///     <ol>
///       <li>@ref GraphFuncIndexGraphConstructor "Graph constructor"</li>
///       <li>@ref GraphFuncIndexGraphAddEdge "Graph.addEdge"</li>
///       <li>@ref GraphFuncIndexGraphAddVertex "Graph.addVertex"</li>
///       <li>@ref GraphFuncIndexGraphGetVertex "Graph.getVertex"</li>
///     </ol>
///   <li>Vertex</li>
///     <ol>
///       <li>@ref GraphFuncIndexVertexEdges "Vertex.edges"</li>
///       <li>@ref GraphFuncIndexVertexAddInEdge "Vertex.addInEdge"</li>
///       <li>@ref GraphFuncIndexVertexAddOutEdge "Vertex.addOutEdge"</li>
///       <li>@ref GraphFuncIndexVertexGetId "Vertex.getId"</li>
///       <li>@ref GraphFuncIndexVertexGetInEdges "Vertex.getInEdges"</li>
///       <li>@ref GraphFuncIndexVertexGetOutEdges "Vertex.getOutEdges"</li>
///       <li>@ref GraphFuncIndexVertexGetProperty "Vertex.getProperty"</li>
///       <li>@ref GraphFuncIndexVertexGetPropertyKeys "Vertex.getPropertyKeys"</li>
///       <li>@ref GraphFuncIndexVertexProperties "Vertex.properties"</li>
///       <li>@ref GraphFuncIndexVertexSetProperty "Vertex.setProperty"</li>
///     </ol>
///   <li>Edge</li>
///     <ol>
///       <li>@ref GraphFuncIndexEdgeGetId "Edge.getId"</li>
///       <li>@ref GraphFuncIndexEdgeGetInVertex "Edge.getInVertex"</li>
///       <li>@ref GraphFuncIndexEdgeGetLabel "Edge.getLabel"</li>
///       <li>@ref GraphFuncIndexEdgeGetOutVertex "Edge.getOutVertex"</li>
///       <li>@ref GraphFuncIndexEdgeGetProperty "Edge.getProperty"</li>
///       <li>@ref GraphFuncIndexEdgeGetPropertyKeys "Edge.getPropertyKeys"</li>
///       <li>@ref GraphFuncIndexEdgeProperties "Edge.properties"</li>
///       <li>@ref GraphFuncIndexEdgeSetProperty "Edge.setProperty"</li>
///     </ol>
///   </li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page GraphFuncIndex Index of Graph Functions
///
/// @copydoc GraphFuncIndexTOC
///
/// @section Graph
///
/// @anchor GraphFuncIndexGraphConstructor
/// @copydetails JSF_Graph
///
/// @anchor GraphFuncIndexGraphAddEdge
/// @copydetails JSF_Graph_prototype_addEdge
///
/// @anchor GraphFuncIndexGraphAddVertex
/// @copydetails JSF_Graph_prototype_addVertex
///
/// @anchor GraphFuncIndexGraphGetVertex
/// @copydetails JSF_Graph_prototype_getVertex
///
/// @section Vertex
///
/// @anchor GraphFuncIndexVertexEdges
/// @copydetails JSF_Vertex_prototype_edges
///
/// @anchor GraphFuncIndexVertexAddInEdge
/// @copydetails JSF_Vertex_prototype_addInEdge
///
/// @anchor GraphFuncIndexVertexAddOutEdge
/// @copydetails JSF_Vertex_prototype_addOutEdge
///
/// @anchor GraphFuncIndexVertexGetId
/// @copydetails JSF_Vertex_prototype_getId
///
/// @anchor GraphFuncIndexVertexGetInEdges
/// @copydetails JSF_Vertex_prototype_getInEdges
///
/// @anchor GraphFuncIndexVertexGetOutEdges
/// @copydetails JSF_Vertex_prototype_getOutEdges
///
/// @anchor GraphFuncIndexVertexGetProperty
/// @copydetails JSF_Vertex_prototype_getProperty
///
/// @anchor GraphFuncIndexVertexGetPropertyKeys
/// @copydetails JSF_Vertex_prototype_getPropertyKeys
///
/// @anchor GraphFuncIndexVertexProperties
/// @copydetails JSF_Vertex_prototype_properties
///
/// @anchor GraphFuncIndexVertexSetProperty
/// @copydetails JSF_Vertex_prototype_setProperty
///
/// @section Edge
///
/// @anchor GraphFuncIndexEdgeGetId
/// @copydetails JSF_Edge_prototype_getId
///
/// @anchor GraphFuncIndexEdgeGetInVertex
/// @copydetails JSF_Edge_prototype_getInVertex
///
/// @anchor GraphFuncIndexEdgeGetLabel
/// @copydetails JSF_Edge_prototype_getLabel
///
/// @anchor GraphFuncIndexEdgeGetOutVertex
/// @copydetails JSF_Edge_prototype_getOutVertex
///
/// @anchor GraphFuncIndexEdgeGetProperty
/// @copydetails JSF_Edge_prototype_getProperty
///
/// @anchor GraphFuncIndexEdgeGetPropertyKeys
/// @copydetails JSF_Edge_prototype_getPropertyKeys
///
/// @anchor GraphFuncIndexEdgeProperties
/// @copydetails JSF_Edge_prototype_properties
///
/// @anchor GraphFuncIndexEdgeSetProperty
/// @copydetails JSF_Edge_prototype_setProperty
///
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                              EDGE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new edge object
////////////////////////////////////////////////////////////////////////////////

function Edge (graph, id) {
  this._graph = graph;
  this._id = id;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the identifier of an edge
///
/// @FUN{@FA{edge}.getId()}
///
/// Returns the identifier of the @FA{edge}.
///
/// @verbinclude graph13
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getId = function (name) {
  return this._id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the to vertex
///
/// @FUN{@FA{edge}.getInVertex()}
///
/// Returns the vertex at the head of the @FA{edge}.
///
/// @verbinclude graph21
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getInVertex = function () {
  return this.graph.constructVertex(this._to);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief label of an edge
///
/// @FUN{@FA{edge}.getLabel()}
///
/// Returns the label of the @FA{edge}.
///
/// @verbinclude graph20
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getLabel = function () {
  if (! this.hasOwnProperty("_label")) {
    this.properties();
  }

  return this._label;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the from vertex
///
/// @FUN{@FA{edge}.getOutVertex()}
///
/// Returns the vertex at the tail of the @FA{edge}.
///
/// @verbinclude graph22
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getOutVertex = function () {
  return this._graph.constructVertex(this._from);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a property of an edge
///
/// @FUN{@FA{edge}.getProperty(@FA{name})}
///
/// Returns the property @FA{name} an @FA{edge}.
///
/// @verbinclude graph12
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getProperty = function (name) {
  var props;

  props = this.properties();

  return props[name]
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all property names of an edge
///
/// @FUN{@FA{vertex}.getPropertyKeys()}
///
/// Returns all propety names a @FA{vertex}.
///
/// @verbinclude graph7
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getPropertyKeys = function () {
  var keys;
  var key;
  var props;

  props = this.properties();
  keys = [];

  for (key in props) {
    if (props.hasOwnProperty(key)) {
      keys.push(key);
    }
  }

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of an edge
///
/// @FUN{@FA{edge}.setProperty(@FA{name}, @FA{value})}
///
/// Changes or sets the property @FA{name} an @FA{edges} to @FA{value}.
///
/// @verbinclude graph14
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.setProperty = function (name, value) {
  var query;
  var props;

  delete this._properties;

  query = this._graph._edges.document(this._id); // TODO use "update"

  if (query.hasNext()) {
    props = query.next();

    props[name] = value;

    this._graph._edges.replace(this._id, props);

    return value;
  }
  else {
    return undefined;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all properties of an edge
///
/// @FUN{@FA{edge}.properties()}
///
/// Returns all properties and their values of an @FA{edge}
///
/// @verbinclude graph11
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.properties = function () {
  var query;
  var prop;

  if (! this.hasOwnProperty("_properties")) {
    query = this._graph._edges.document(this._id);

    if (query.hasNext()) {
      this._properties = query.next();
      this._label = this._properties._label;
      this._from = this._properties._from;
      this._to = this._properties._to;

      delete this._properties._id;
      delete this._properties._label;
      delete this._properties._from;
      delete this._properties._to;
    }
    else {
      return undefined;
    }
  }

  return this._properties;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief edge printing
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.PRINT = function () {
  internal.output("Edge(<graph>, \"", this._id, "\")");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            VERTEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new vertex object
////////////////////////////////////////////////////////////////////////////////

function Vertex (graph, id) {
  this._graph = graph;
  this._id = id;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an inbound edge
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

Vertex.prototype.addInEdge = function (out, label, data) {
  return this._graph.addEdge(out, this, label, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an outbound edge
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer}, @FA{label})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} with given @FA{label} and
/// returns the edge object.
///
/// @verbinclude graph23
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer}, @FA{label}, @FA{data})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} with given @FA{label} and
/// properties defined in @FA{data}. Returns the edge object.
///
/// @verbinclude graph24
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.addOutEdge = function (ine, label, data) {
  return this._graph.addEdge(this, ine, label, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound and outbound edges
///
/// @FUN{@FA{vertex}.edges()}
///
/// Returns a list of in- or outbound edges of the @FA{vertex}.
///
/// @verbinclude graph15
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.edges = function () {
  var query;
  var result;
  var graph;

  if (! this.hasOwnProperty("_edges")) {
    graph = this._graph;
    query = graph._vertices.document(this._id).edges(graph._edges);
    result = [];

    while (query.hasNext()) {
      result.push(graph.constructEdge(query.nextRef()));
    }

    this._edges = result;
  }

  return this._edges;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the identifier of a vertex
///
/// @FUN{@FA{vertex}.getId()}
///
/// Returns the identifier of the @FA{vertex}.
///
/// @verbinclude graph8
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getId = function (name) {
  return this._id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges with given label
///
/// @FUN{@FA{vertex}.getInEdges(@FA{label}, ...)}
///
/// Returns a list of inbound edges of the @FA{vertex} with given label(s).
///
/// @verbinclude graph18
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getInEdges = function () {
  var edges;
  var labels;
  var result;
  var edge;

  if (arguments.length == 0) {
    return this.inbound();
  }
  else {
    labels = {};

    for (var i = 0;  i < arguments.length;  ++i) {
      labels[arguments[i]] = true;
    }

    edges = this.inbound();
    result = [];

    for (var i = 0;  i < edges.length;  ++i) {
      if (edges[i].getLabel() in labels) {
        result.push(edges[i]);
      }
    }

    return result;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges with given label
///
/// @FUN{@FA{vertex}.getOutEdges(@FA{label}, ...)}
///
/// Returns a list of outbound edges of the @FA{vertex} with given label(s).
///
/// @verbinclude graph19
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getOutEdges = function () {
  var edges;
  var labels;
  var result;
  var edge;

  if (arguments.length == 0) {
    return this.outbound();
  }
  else {
    labels = {};

    for (var i = 0;  i < arguments.length;  ++i) {
      labels[arguments[i]] = true;
    }

    edges = this.outbound();
    result = [];

    for (var i = 0;  i < edges.length;  ++i) {
      if (edges[i].getLabel() in labels) {
        result.push(edges[i]);
      }
    }

    return result;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a property of a vertex
///
/// @FUN{@FA{vertex}.getProperty(@FA{name})}
///
/// Returns the property @FA{name} a @FA{vertex}.
///
/// @verbinclude graph5
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getProperty = function (name) {
  var props;

  props = this.properties();

  return props[name]
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all property names of a vertex
///
/// @FUN{@FA{vertex}.getPropertyKeys()}
///
/// Returns all propety names a @FA{vertex}.
///
/// @verbinclude graph7
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getPropertyKeys = function () {
  var keys;
  var key;
  var props;

  props = this.properties();
  keys = [];

  for (key in props) {
    if (props.hasOwnProperty(key)) {
      keys.push(key);
    }
  }

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges
///
/// @FUN{@FA{vertex}.inbound()}
///
/// Returns a list of inbound edges of the @FA{vertex}.
///
/// @verbinclude graph16
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inbound = function () {
  var query;
  var result;
  var graph;

  if (! this.hasOwnProperty("_inbound")) {
    graph = this._graph;
    query = graph._vertices.document(this._id).inEdges(graph._edges);
    result = [];

    while (query.hasNext()) {
      result.push(graph.constructEdge(query.nextRef()));
    }

    this._inbound = result;
  }

  return this._inbound;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges
///
/// @FUN{@FA{vertex}.outbound()}
///
/// Returns a list of outbound edges of the @FA{vertex}.
///
/// @verbinclude graph17
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outbound = function () {
  var query;
  var result;
  var graph;

  if (! this.hasOwnProperty("_outbound")) {
    graph = this._graph;
    query = graph._vertices.document(this._id).outEdges(graph._edges);
    result = [];

    while (query.hasNext()) {
      result.push(graph.constructEdge(query.nextRef()));
    }

    this._outbound = result;
  }

  return this._outbound;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all properties of a vertex
///
/// @FUN{@FA{vertex}.properties()}
///
/// Returns all properties and their values of a @FA{vertex}
///
/// @verbinclude graph4
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.properties = function () {
  var query;
  var prop;

  if (! this.hasOwnProperty("_properties")) {
    query = this._graph._vertices.document(this._id);

    if (query.hasNext()) {
      this._properties = query.next();
      delete this._properties._id;
    }
    else {
      return undefined;
    }
  }

  return this._properties;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of a vertex
///
/// @FUN{@FA{vertex}.setProperty(@FA{name}, @FA{value})}
///
/// Changes or sets the property @FA{name} a @FA{vertex} to @FA{value}.
///
/// @verbinclude graph6
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.setProperty = function (name, value) {
  var query;
  var props;

  delete this._properties;

  query = this._graph._vertices.document(this._id); // TODO use "update"

  if (query.hasNext()) {
    props = query.next();

    props[name] = value;

    this._graph._vertices.replace(this._id, props);

    return value;
  }
  else {
    return undefined;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex representation
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.PRINT = function () {
  internal.output("Vertex(<graph>, \"", this._id, "\")");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             GRAPH
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new graph object
///
/// @FUN{Graph(@FA{vertices}, @FA{edges})}
///
/// Constructs a new graph object using the collection @FA{vertices} for all
/// vertices and the collection @FA{edges} for all edges.
///
/// @verbinclude graph1
////////////////////////////////////////////////////////////////////////////////

function Graph (vertices, edg) {
  if (typeof vertices === "string") {
    vertices = db[vertices];
  }
  else if (! vertices instanceof AvocadoCollection) {
    throw "<vertices> must be a document collection";
  }

  if (typeof edg === "string") {
    edg = edges[edg];
  }
  else if (! edg instanceof AvocadoEdgesCollection) {
    throw "<edges> must be an edges collection";
  }

  this._vertices = vertices;
  this._verticesCache = {};

  this._edges = edg;
  this._edgesCache = {};
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an edge to the graph
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in})}
///
/// Creates a new edge from @FA{out} to @FA{in} and returns the edge
/// object.
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
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{label}, @FA{data})}
///
/// Creates a new edge and returns the edge object. The edge has the
/// label @FA{label} and contains the properties defined in @FA{data}.
///
/// @verbinclude graph10
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addEdge = function (out, ine, label, data) {
  var ref;
  var shallow;
  var key;
  var edge;

  if (typeof label === 'object') {
    data = label;
    label = undefined;
  }

  if (label === undefined) {
    label = null;
  }

  if (data === undefined) {
    ref = this._edges.save(out._id, ine._id, {"_label" : label});
  }
  else {
    shallow = {};

    for (key in data) {
      if (data.hasOwnProperty(key)) {
        shallow[key] = data[key];
      }
    }

    shallow["_label"] = label;

    ref = this._edges.save(out._id, ine._id, shallow);
  }

  edge = this.constructEdge(ref);

  if (out.hasOwnProperty("_edges")) {
    out._edges.push(edge);
  }

  if (out.hasOwnProperty("_outbound")) {
    out._outbound.push(edge);
  }

  if (ine.hasOwnProperty("_edges")) {
    ine._edges.push(edge);
  }

  if (ine.hasOwnProperty("_inbound")) {
    ine._inbound.push(edge);
  }

  return edge;
}

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

Graph.prototype.addVertex = function (data) {
  var ref;

  if (data === undefined) {
    ref = this._vertices.save({});
  }
  else {
    ref = this._vertices.save(data);
  }

  return this.constructVertex(ref);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a vertex given its id
///
/// @FUN{@FA{graph}.getVertex(@FA{id})}
///
/// Returns the vertex identified by @FA{id} or undefined.
///
/// @verbinclude graph2
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertex = function (id) {
  var ref;

  ref = this._vertices.document(id);

  if (ref.count() == 1) {
    return this.constructVertex(id);
  }
  else {
    return undefined;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief private function to construct a vertex
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.constructVertex = function(id) {
  if (! (id in this._verticesCache)) {
    this._verticesCache[id] = new Vertex(this, id);
  }

  return this._verticesCache[id];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief private function to construct an edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.constructEdge = function(id) {
  if (! (id in this._edgesCache)) {
    this._edgesCache[id] = new Edge(this, id);
  }

  return this._edgesCache[id];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief graph printing
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.PRINT = function () {
  internal.output("Graph(\"", this._vertices._name, "\", \"" + this._edges._name, "\")");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
