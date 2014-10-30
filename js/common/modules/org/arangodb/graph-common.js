/*jshint strict: false */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var is = require("org/arangodb/is"),
  Edge,
  Graph,
  Vertex,
  GraphArray,
  Iterator;

// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/graph-blueprint"
// -----------------------------------------------------------------------------

Iterator = function (wrapper, cursor, stringRepresentation) {
  this.next = function next() {
    if (cursor.hasNext()) {
      return wrapper(cursor.next());
    }

    return undefined;
  };

  this.hasNext = function hasNext() {
    return cursor.hasNext();
  };

  this._PRINT = function (context) {
    context.output += stringRepresentation;
  };
};


// -----------------------------------------------------------------------------
// --SECTION--                                                        GraphArray
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a graph arrays
////////////////////////////////////////////////////////////////////////////////

GraphArray = function (len) {
  if (len !== undefined) {
    this.length = len;
  }
};

GraphArray.prototype = new Array(0);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief map
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.map = function (fun, thisp) {
  var len = this.length;
  var i;

  if (typeof fun !== "function") {
    throw new TypeError();
  }

  var res = new GraphArray(len);

  for (i = 0;  i < len;  i++) {
    if (this.hasOwnProperty(i)) {
      res[i] = fun.call(thisp, this[i], i, this);
    }
  }

  return res;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the in vertices
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getInVertex = function () {
  return this.map(function(a) {return a.getInVertex();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the out vertices
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getOutVertex = function () {
  return this.map(function(a) {return a.getOutVertex();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the peer vertices
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getPeerVertex = function (vertex) {
  return this.map(function(a) {return a.getPeerVertex(vertex);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the property
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.setProperty = function (name, value) {
  return this.map(function(a) {return a.setProperty(name, value);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.edges = function () {
  return this.map(function(a) {return a.edges();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get outbound edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.outbound = function () {
  return this.map(function(a) {return a.outbound();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get inbound edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.inbound = function () {
  return this.map(function(a) {return a.inbound();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the in edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getInEdges = function () {
  var args = arguments;
  return this.map(function(a) {return a.getInEdges.apply(a, args);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the out edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getOutEdges = function () {
  var args = arguments;
  return this.map(function(a) {return a.getOutEdges.apply(a, args);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getEdges = function () {
  var args = arguments;
  return this.map(function(a) {return a.getEdges.apply(a, args);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.degree = function () {
  return this.map(function(a) {return a.degree();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of in-edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.inDegree = function () {
  return this.map(function(a) {return a.inDegree();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of out-edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.inDegree = function () {
  return this.map(function(a) {return a.outDegree();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the properties
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.properties = function () {
  return this.map(function(a) {return a.properties();});
};

// -----------------------------------------------------------------------------
// --SECTION--                                                              Edge
// -----------------------------------------------------------------------------

Edge = function (graph, properties) {
  this._graph = graph;
  this._id = properties._key;
  this._properties = properties;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock edgeGetId
///
/// `edge.getId()`
///
/// Returns the identifier of the *edge*.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-id
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getId = function () {
  return this._properties._key;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock edgeGetLabel
///
/// `edge.getLabel()`
///
/// Returns the label of the *edge*.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-label
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getLabel = function () {
  return this._properties.$label;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock edgeGetProperty
///
/// `edge.getProperty(edge)`
///
/// Returns the property *edge* an *edge*.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-property
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getProperty = function (name) {
  return this._properties[name];
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock edgeGetPropertyKeys
///
/// `edge.getPropertyKeys()`
///
/// Returns all propety names an *edge*.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-property-keys
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getPropertyKeys = function () {
  return this._properties.propertyKeys;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock edgeProperties
///
/// `edge.properties()`
///
/// Returns all properties and their values of an *edge*
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-properties
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.properties = function () {
  return this._properties._shallowCopy;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock edgeGetInVertex
///
/// `edge.getInVertex()`
///
/// Returns the vertex at the head of the *edge*.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-in-vertex
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getInVertex = function () {
  return this._graph.getVertex(this._properties._to);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock edgeGetOutVertex
///
/// `edge.getOutVertex()`
///
/// Returns the vertex at the tail of the *edge*.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-out-vertex
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getOutVertex = function () {
  return this._graph.getVertex(this._properties._from);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock edgeGetPeerVertex
///
/// `edge*.getPeerVertex(vertex)`
///
/// Returns the peer vertex of the *edge* and the *vertex*.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{edgeGetPeerVertex}
/// ~ db._create("example");
///   v1 = example.addVertex("1");
///   v2 = g.addVertex("2");
///   e = g.addEdge(v1, v2, "1->2", "knows");
///   e.getPeerVertex(v1);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getPeerVertex = function (vertex) {
  if (vertex._properties._id === this._properties._to) {
    return this._graph.getVertex(this._properties._from);
  }

  if (vertex._properties._id === this._properties._from) {
    return this._graph.getVertex(this._properties._to);
  }

  return null;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief edge printing
////////////////////////////////////////////////////////////////////////////////

Edge.prototype._PRINT = function (context) {
  if (!this._properties._id) {
    context.output += "[deleted Edge]";
  }
  else if (this._properties._key !== undefined) {
    if (typeof this._properties._key === "string") {
      context.output += "Edge(\"" + this._properties._key + "\")";
    }
    else {
      context.output += "Edge(" + this._properties._key + ")";
    }
  }
  else {
    context.output += "Edge(<" + this._id + ">)";
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                            Vertex
// -----------------------------------------------------------------------------

Vertex = function (graph, properties) {
  this._graph = graph;
  this._id = properties._key;
  this._properties = properties;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock vertexAddInEdge
///
/// `vertex.addInEdge(peer, id)`
///
/// Creates a new edge from *peer* to *peer* and returns the edge
/// object. The identifier *peer* must be a unique identifier or null.
///
/// `peer.addInEdge(peer, peer, label)`
///
/// Creates a new edge from *peer* to *peer* with given label and
/// returns the edge object.
///
/// `peer.addInEdge(peer, peer, label, peer)`
///
/// Creates a new edge from *peer* to *peer* with given label and
/// properties defined in *peer*. Returns the edge object.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-add-in-edge
///
/// @verbinclude graph-vertex-add-in-edge2
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.addInEdge = function (out, id, label, data) {
  return this._graph.addEdge(out, this, id, label, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock peerAddOutEdge
///
/// `peer.addOutEdge(peer)`
///
/// Creates a new edge from *peer* to *peer* and returns the edge
/// object.
///
/// `peer.addOutEdge(peer, label)`
///
/// Creates a new edge from *peer* to *peer* with given *label* and
/// returns the edge object.
///
/// `peer.addOutEdge(peer, label, peer)`
///
/// Creates a new edge from *peer* to *peer* with given *label* and
/// properties defined in *peer*. Returns the edge object.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-add-out-edge
///
/// @verbinclude graph-vertex-add-out-edge2
///
/// @verbinclude graph-vertex-add-out-edge3
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.addOutEdge = function (ine, id, label, data) {
  return this._graph.addEdge(this, ine, id, label, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.degree = function () {
  return this.getEdges().length;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of in-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inDegree = function () {
  return this.getInEdges().length;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of out-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outDegree = function () {
  return this.getOutEdges().length;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock peerGetId
///
/// `peer.getId()`
///
/// Returns the identifier of the *peer*. If the vertex was deleted, then
/// *undefined* is returned.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-id
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getId = function () {
  return this._properties._key;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock peerGetProperty
///
/// `peer.getProperty(edge)`
///
/// Returns the property *edge* a *peer*.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-property
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getProperty = function (name) {
  return this._properties[name];
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock peerGetPropertyKeys
///
/// `peer.getPropertyKeys()`
///
/// Returns all propety names a *peer*.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-property-keys
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getPropertyKeys = function () {
  return this._properties.propertyKeys;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock peerproperties
///
/// `peer.properties()`
///
/// Returns all properties and their values of a *peer*
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-properties
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.properties = function () {
  return this._properties._shallowCopy;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex representation
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype._PRINT = function (context) {
  if (! this._properties._id) {
    context.output += "[deleted Vertex]";
  }
  else if (this._properties._key !== undefined) {
    if (typeof this._properties._key === "string") {
      context.output += "Vertex(\"" + this._properties._key + "\")";
    }
    else {
      context.output += "Vertex(" + this._properties._key + ")";
    }
  }
  else {
    context.output += "Vertex(<" + this._id + ">)";
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                             Graph
// -----------------------------------------------------------------------------

Graph = function (name, vertices, edges, waitForSync) {
  this.initialize(name, vertices, edges, waitForSync);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

Graph.prototype._prepareEdgeData = function (data, label) {
  var edgeData;

  if (is.notExisty(data) && is.object(label)) {
    data = label;
    label = null;
  }

  if (is.notExisty(label) && is.existy(data) && is.existy(data.$label)) {
    label = data.$label;
  }

  if (is.notExisty(data) || is.noObject(data)) {
    edgeData = {};
  } else {
    edgeData = data._shallowCopy || {};
  }

  edgeData.$label = label;

  return edgeData;
};

Graph.prototype._prepareVertexData = function (data) {
  var vertexData;

  if (is.notExisty(data) || is.noObject(data)) {
    vertexData = {};
  } else {
    vertexData = data._shallowCopy || {};
  }

  return vertexData;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a vertex from the graph, create it if it doesn't exist
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getOrAddVertex = function (id) {
  var v = this.getVertex(id);

  if (v === null) {
    v = this.addVertex(id);
  }

  return v;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock graphAddEdge
///
/// `graph.addEdge(out, in, peer)`
///
/// Creates a new edge from *out* to *out* and returns the edge object. The
/// identifier *peer* must be a unique identifier or null.
/// out and in can either be vertices or their IDs
///
/// `graph.addEdge(out, out, peer, label)`
///
/// Creates a new edge from *out* to *out* with *label* and returns the
/// edge object.
/// out and in can either be vertices or their IDs
///
/// `graph.addEdge(out, out, peer, peer)`
///
/// Creates a new edge and returns the edge object. The edge contains the
/// properties defined in *peer*.
/// out and in can either be vertices or their IDs
///
/// `graph.addEdge(out, out, peer, label, peer)`
///
/// Creates a new edge and returns the edge object. The edge has the
/// label *label* and contains the properties defined in *peer*.
/// out and in can either be vertices or their IDs
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-add-edge
///
/// @verbinclude graph-graph-add-edge2
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addEdge = function (out_vertex, in_vertex, id, label, data, waitForSync) {
  var out_vertex_id, in_vertex_id;

  if (is.string(out_vertex)) {
    out_vertex_id = out_vertex;
  } else {
    out_vertex_id = out_vertex._properties._id;
  }

  if (is.string(in_vertex)) {
    in_vertex_id = in_vertex;
  } else {
    in_vertex_id = in_vertex._properties._id;
  }

  return this._saveEdge(id,
                        out_vertex_id,
                        in_vertex_id,
                        this._prepareEdgeData(data, label),
                        waitForSync);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock graphAddVertex
///
/// `graph.addVertex(peer)`
///
/// Creates a new vertex and returns the vertex object. The identifier
/// *peer* must be a unique identifier or null.
///
/// `graph.addVertex(peer, peer)`
///
/// Creates a new vertex and returns the vertex object. The vertex contains
/// the properties defined in *peer*.
///
/// @EXAMPLES
///
/// Without any properties:
///
/// @verbinclude graph-graph-add-vertex
///
/// With given properties:
///
/// @verbinclude graph-graph-add-vertex2
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addVertex = function (id, data, waitForSync) {
  return this._saveVertex(id, this._prepareVertexData(data), waitForSync);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces an existing vertex by ID
///
/// @FUN{@FA{graph}.replaceVertex(*peer*, *peer*)}
///
/// Replaces an existing vertex by ID
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.replaceVertex = function (id, data) {
  this._replaceVertex(id, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock graphReplaceEdge
///
/// `graph.replaceEdge(peer, peer)`
///
/// Replaces an existing edge by ID
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.replaceEdge = function (id, data) {
  this._replaceEdge(id, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of vertices
///
/// @FUN{@FA{graph}.order()}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.order = function () {
  return this._vertices.count();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of edges
///
/// @FUN{@FA{graph}.size()}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.size = function () {
  return this._edges.count();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief empties the internal cache for Predecessors
///
/// @FUN{@FA{graph}.emptyCachedPredecessors()}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.emptyCachedPredecessors = function () {
  this.predecessors = {};
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets Predecessors for a pair from the internal cache
///
/// @FUN{@FA{graph}.getCachedPredecessors(@FA{target}), @FA{source})}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getCachedPredecessors = function (target, source) {
  var predecessors;

  if (this.predecessors[target.getId()]) {
    predecessors = this.predecessors[target.getId()][source.getId()];
  }

  return predecessors;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets Predecessors for a pair in the internal cache
///
/// @FUN{@FA{graph}.setCachedPredecessors(@FA{target}), @FA{source}, @FA{value})}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.setCachedPredecessors = function (target, source, value) {
  if (!this.predecessors[target.getId()]) {
    this.predecessors[target.getId()] = {};
  }

  this.predecessors[target.getId()][source.getId()] = value;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a vertex
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.constructVertex = function (data) {
  var id, rev;

  if (typeof data === "string") {
    id = data;
  } else {
    id = data._id;
    rev = data._rev;
  }

  var vertex = this._verticesCache[id];

  if (vertex === undefined || vertex._rev !== rev) {
    var properties = this._vertices.document(id);

    if (! properties) {
      throw "accessing a deleted vertex";
    }

    this._verticesCache[id] = vertex = new Vertex(this, properties);
  }

  return vertex;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.constructEdge = function (data) {
  var id, rev, edge, properties;

  if (typeof data === "string") {
    id = data;
  } else {
    id = data._id;
    rev = data._rev;
  }

  edge = this._edgesCache[id];

  if (edge === undefined || edge._rev !== rev) {
    properties = this._edges.document(id);

    if (!properties) {
      throw "accessing a deleted edge";
    }

    this._edgesCache[id] = edge = new Edge(this, properties);
  }

  return edge;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief graph printing
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._PRINT = function (context) {
  context.output += "Graph(\"" + this._properties._key + "\")";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.Edge = Edge;
exports.Graph = Graph;
exports.Vertex = Vertex;
exports.GraphArray = GraphArray;
exports.Iterator = Iterator;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
