////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page GraphFuncIndexTOC
///
/// <ol>
///   <li>Graph</li>
///     <ol>
///       <li>Graph constructor</li>
///       <li>Graph.addEdge</li>
///       <li>Graph.addVertex</li>
///     </ol>
///   <li>Vertex</li>
///     <ol>
///       <li>Vertex.getId</li>
///       <li>Vertex.getInEdges</li>
///       <li>Vertex.getOutEdges</li>
///       <li>Vertex.getProperty</li>
///       <li>Vertex.getPropertyKeys</li>
///       <li>Vertex.setProperty</li>
///       <li>Vertex.edges</li>
///       <li>Vertex.properties</li>
///     </ol>
///   <li>Edge</li>
///     <ol>
///       <li>Edge.getId</li>
///       <li>Edge.getInVertex</li>
///       <li>Edge.getLabel</li>
///       <li>Edge.getOutVertex</li>
///       <li>Edge.getProperty</li>
///       <li>Edge.getPropertyKeys</li>
///       <li>Edge.setProperty</li>
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
/// @copydetails JSF_Graph
///
/// @copydetails JSF_Graph_prototype_addEdge
///
/// @copydetails JSF_Graph_prototype_addVertex
///
/// @copydetails JSF_Graph_prototype_getVertex
///
/// @section Vertex
///
/// @copydetails JSF_Vertex_prototype_getId
///
/// @copydetails JSF_Vertex_prototype_getInEdges
///
/// @copydetails JSF_Vertex_prototype_getOutEdges
///
/// @copydetails JSF_Vertex_prototype_getProperty
///
/// @copydetails JSF_Vertex_prototype_getPropertyKeys
///
/// @copydetails JSF_Vertex_prototype_setProperty
///
/// @copydetails JSF_Vertex_prototype_edges
///
/// @copydetails JSF_Vertex_prototype_properties
///
/// @section Edge
///
/// @copydetails JSF_Edge_prototype_getId
///
/// @copydetails JSF_Edge_prototype_getInVertex
///
/// @copydetails JSF_Edge_prototype_getLabel
///
/// @copydetails JSF_Edge_prototype_getOutVertex
///
/// @copydetails JSF_Edge_prototype_getProperty
///
/// @copydetails JSF_Edge_prototype_getPropertyKeys
///
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
  output("Edge(<graph>, \"", this._id, "\")");
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
  output("Vertex(<graph>, \"", this._id, "\")");
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
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{label})}
///
/// Creates a new edge from @FA{out} to @FA{in} with @FA{label} and returns the
/// edge object.
///
/// @verbinclude graph9
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{label}, @FA{data})}
///
/// Creates a new edge and returns the edge object. The edge contains the
/// properties defined in @FA{data}.
///
/// @verbinclude graph10
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addEdge = function (out, ine, label, data) {
  var ref;
  var shallow;
  var key;
  var edge;

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
  output("Graph(\"", this._vertices._name, "\", \"" + this._edges._name, "\")");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
