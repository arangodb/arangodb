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
var db = internal.db;
var edges = internal.edges;
var AvocadoCollection = internal.AvocadoCollection;
var AvocadoEdgesCollection = internal.AvocadoEdgesCollection;

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

// -----------------------------------------------------------------------------
// --SECTION--                                                              EDGE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new edge object
////////////////////////////////////////////////////////////////////////////////

function Edge (graph, id) {
  var props;

  this._graph = graph;
  this._id = id;

  props = this._graph._edges.document(this._id).next();

  if (props) {
    this._label = props._label;
    this._from = props._from;
    this._to = props._to;
  }
  else {
    this._id = undefined;
  }
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
  if (! this._id) {
    throw "accessing a deleted edge";
  }

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
  if (! this._id) {
    throw "accessing a deleted edge";
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
  if (! this._id) {
    throw "accessing a deleted edge";
  }

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
  if (! this._id) {
    throw "accessing a deleted edge";
  }

  return this.properties()[name];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all property names of an edge
///
/// @FUN{@FA{edge}.getPropertyKeys()}
///
/// Returns all propety names an @FA{edge}.
///
/// @verbinclude graph32
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getPropertyKeys = function () {
  var keys;
  var key;
  var props;

  if (! this._id) {
    throw "accessing a deleted edge";
  }

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

  if (! this._id) {
    throw "accessing a deleted edge";
  }

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
  var props;

  if (! this._id) {
    throw "accessing a deleted edge";
  }

  props = this._graph._edges.document(this._id).next();

  if (! props) {
    this._id = undefined;
    throw "accessing a deleted edge";
  }

  delete props._id;
  delete props._label;
  delete props._from;
  delete props._to;

  return props;
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

Edge.prototype._PRINT = function (seen, path, names) {
  if (! this._id) {
    internal.output("[deleted Edge]");
  }
  else {
    internal.output("Edge(<graph>, \"", this._id, "\")");
  }
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
  var props;

  this._graph = graph;
  this._id = id;

  props = this._graph._vertices.document(this._id);

  if (! props.hasNext()) {
    this._id = undefined;
  }
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

Vertex.prototype.addInEdge = function (out, label, data) {
  if (! this._id) {
    throw "accessing a deleted vertex";
  }

  return this._graph.addEdge(out, this, label, data);
}

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

Vertex.prototype.addOutEdge = function (ine, label, data) {
  if (! this._id) {
    throw "accessing a deleted vertex";
  }

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

  if (! this._id) {
    throw "accessing a deleted vertex";
  }

  graph = this._graph;
  query = graph._vertices.document(this._id).edges(graph._edges);
  result = [];

  while (query.hasNext()) {
    result.push(graph.constructEdge(query.nextRef()));
  }

  return result;
}

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

  if (! this._id) {
    throw "accessing a deleted vertex";
  }

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

  if (! this._id) {
    throw "accessing a deleted vertex";
  }

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
  if (! this._id) {
    throw "accessing a deleted vertex";
  }

  return this.properties()[name];
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

  if (! this._id) {
    throw "accessing a deleted vertex";
  }

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

  if (! this._id) {
    throw "accessing a deleted vertex";
  }

  graph = this._graph;
  query = graph._vertices.document(this._id).inEdges(graph._edges);
  result = [];

  while (query.hasNext()) {
    result.push(graph.constructEdge(query.nextRef()));
  }

  return result;
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

  if (! this._id) {
    throw "accessing a deleted vertex";
  }

  graph = this._graph;
  query = graph._vertices.document(this._id).outEdges(graph._edges);
  result = [];

  while (query.hasNext()) {
    result.push(graph.constructEdge(query.nextRef()));
  }

  return result;
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
  var props;

  if (! this._id) {
    throw "accessing a deleted vertex";
  }

  props = this._graph._vertices.document(this._id).next();

  if (! props) {
    this._id = undefined;
    throw "accessing a deleted vertex";
  }

  delete props._id;

  return props;
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

  if (! this._id) {
    throw "accessing a deleted vertex";
  }

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

Vertex.prototype._PRINT = function (seen, path, names) {
  if (! this._id) {
    internal.output("[deleted Vertex]");
  }
  else {
    internal.output("Vertex(<graph>, \"", this._id, "\")");
  }
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
/// vertices and the collection @FA{edges} for all edges. Note that it is
/// possible to construct two graphs with the same vertex set, but different
/// edge sets.
///
/// @verbinclude graph1
////////////////////////////////////////////////////////////////////////////////

function Graph (vertices, edg) {
  if (typeof vertices === "string") {
    vertices = db[vertices];
  }

  if (! vertices instanceof AvocadoCollection) {
    throw "<vertices> must be a document collection";
  }

  if (typeof edg === "string") {
    edg = edges[edg];
  }

  if (! edg instanceof AvocadoEdgesCollection) {
    throw "<edges> must be an edges collection";
  }

  this._vertices = vertices;
  this._edges = edg;
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

  return this.constructEdge(ref);
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
/// @verbinclude graph29
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
/// @brief returns an iterator for all vertices
///
/// @FUN{@FA{graph}.getVertices()}
///
/// Returns an iterator for all vertices of the graph. The iterator supports the
/// methods @FN{hasNext} and @FN{next}.
///
/// @verbinclude graph35
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertices = function () {
  var that;
  var all;

  that = this;
  all = this._vertices.all();

  iterator = function() {
    this.next = function next() {
      var v;

      v = all.next();

      if (v == undefined) {
        return undefined;
      }

      return that.constructVertex(v._id);
    };

    this.hasNext = function hasNext() {
      return all.hasNext();
    };

    this._PRINT = function(seen, path, names) {
      internal.output("[vertex iterator]");
    }
  };

  return new iterator();
}

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

Graph.prototype.getEdges = function () {
  var that;
  var all;

  that = this;
  all = this._edges.all();

  iterator = function() {
    this.next = function next() {
      var v;

      v = all.next();

      if (v == undefined) {
        return undefined;
      }

      return that.constructEdge(v._id);
    };

    this.hasNext = function hasNext() {
      return all.hasNext();
    };

    this._PRINT = function(seen, path, names) {
      internal.output("[edge iterator]");
    }
  };

  return new iterator();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a vertex and all in- or out-bound edges
///
/// @FUN{@FA{graph}.removeVertex(@FA{vertex})}
///
/// Deletes the @FA{vertex} and all its edges.
///
/// @verbinclude graph37
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeVertex = function (vertex) {
  var result;

  if (! vertex._id) {
    return;
  }

  edges = vertex.edges();
  result = this._vertices.delete(vertex._id);

  if (! result) {
    throw "cannot delete vertex";
  }

  vertex._id = undefined;

  for (var i = 0;  i < edges.length;  ++i) {
    this.removeEdge(edges[i]);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an edge
///
/// @FUN{@FA{graph}.removeEdge(@FA{vertex})}
///
/// Deletes the @FA{edge}. Note that the in and out vertices are left untouched.
///
/// @verbinclude graph38
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeEdge = function (edge) {
  var result;

  if (! edge._id) {
    return;
  }

  result = this._edges.delete(edge._id);

  if (! result) {
    throw "cannot delete edge";
  }

  edge._id = undefined;
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
  return new Vertex(this, id);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief private function to construct an edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.constructEdge = function(id) {
  return new Edge(this, id);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief graph printing
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._PRINT = function (seen, path, names) {
  internal.output("Graph(\"", this._vertices._name, "\", \"" + this._edges._name, "\")");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.AvocadoCollection = AvocadoCollection;
exports.AvocadoEdgesCollection = AvocadoEdgesCollection;
exports.Edge = Edge;
exports.Graph = Graph;
exports.Vertex = Vertex;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
