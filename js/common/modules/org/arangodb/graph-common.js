/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
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

var arangodb = require("org/arangodb"),
  is = require("org/arangodb/is"),
  Edge,
  Graph,
  Vertex,
  GraphArray;

// -----------------------------------------------------------------------------
// --SECTION--                                       module "org/arangodb/graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                        GraphArray
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a graph arrays
////////////////////////////////////////////////////////////////////////////////

GraphArray = function (len) {
  if (len !== undefined) {
    this.length = len;
  }
};

GraphArray.prototype = new Array(0);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the identifier of an edge
///
/// @FUN{@FA{edge}.getId()}
///
/// Returns the identifier of the @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-id
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getId = function () {
  return this._properties._key;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief label of an edge
///
/// @FUN{@FA{edge}.getLabel()}
///
/// Returns the label of the @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-label
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getLabel = function () {
  return this._properties.$label;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a property of an edge
///
/// @FUN{@FA{edge}.getProperty(@FA{name})}
///
/// Returns the property @FA{name} an @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-property
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getProperty = function (name) {
  return this._properties[name];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all property names of an edge
///
/// @FUN{@FA{edge}.getPropertyKeys()}
///
/// Returns all propety names an @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-property-keys
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getPropertyKeys = function () {
  return this._properties.propertyKeys;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all properties of an edge
///
/// @FUN{@FA{edge}.properties()}
///
/// Returns all properties and their values of an @FA{edge}
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-properties
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.properties = function () {
  return this._properties._shallowCopy;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the to vertex
///
/// @FUN{@FA{edge}.getInVertex()}
///
/// Returns the vertex at the head of the @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-in-vertex
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getInVertex = function () {
  return this._graph.getVertex(this._properties._to);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the from vertex
///
/// @FUN{@FA{edge}.getOutVertex()}
///
/// Returns the vertex at the tail of the @FA{edge}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-get-out-vertex
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getOutVertex = function () {
  return this._graph.getVertex(this._properties._from);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the other vertex
///
/// @FUN{@FA{edge}.getPeerVertex(@FA{vertex})}
///
/// Returns the peer vertex of the @FA{edge} and the @FA{vertex}.
///
/// @EXAMPLES
///
/// @code
/// arango> v1 = g.addVertex("1");
/// Vertex("1")
///
/// arango> v2 = g.addVertex("2");
/// Vertex("2")
///
/// arango> e = g.addEdge(v1, v2, "1->2", "knows");
/// Edge("1->2")
///
/// arango> e.getPeerVertex(v1);
/// Vertex(2)
/// @endcode
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an inbound edge
///
/// @FUN{@FA{vertex}.addInEdge(@FA{peer}, @FA{id})}
///
/// Creates a new edge from @FA{peer} to @FA{vertex} and returns the edge
/// object. The identifier @FA{id} must be a unique identifier or null.
///
/// @FUN{@FA{vertex}.addInEdge(@FA{peer}, @FA{id}, @FA{label})}
///
/// Creates a new edge from @FA{peer} to @FA{vertex} with given label and
/// returns the edge object.
///
/// @FUN{@FA{vertex}.addInEdge(@FA{peer}, @FA{id}, @FA{label}, @FA{data})}
///
/// Creates a new edge from @FA{peer} to @FA{vertex} with given label and
/// properties defined in @FA{data}. Returns the edge object.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-add-in-edge
///
/// @verbinclude graph-vertex-add-in-edge2
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.addInEdge = function (out, id, label, data) {
  return this._graph.addEdge(out, this, id, label, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an outbound edge
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} and returns the edge
/// object.
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer}, @FA{label})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} with given @FA{label} and
/// returns the edge object.
///
/// @FUN{@FA{vertex}.addOutEdge(@FA{peer}, @FA{label}, @FA{data})}
///
/// Creates a new edge from @FA{vertex} to @FA{peer} with given @FA{label} and
/// properties defined in @FA{data}. Returns the edge object.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-add-out-edge
///
/// @verbinclude graph-vertex-add-out-edge2
///
/// @verbinclude graph-vertex-add-out-edge3
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.addOutEdge = function (ine, id, label, data) {
  return this._graph.addEdge(this, ine, id, label, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the identifier of a vertex
///
/// @FUN{@FA{vertex}.getId()}
///
/// Returns the identifier of the @FA{vertex}. If the vertex was deleted, then
/// @LIT{undefined} is returned.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-id
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getId = function () {
  return this._properties._key;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a property of a vertex
///
/// @FUN{@FA{vertex}.getProperty(@FA{name})}
///
/// Returns the property @FA{name} a @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-property
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getProperty = function (name) {
  return this._properties[name];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all property names of a vertex
///
/// @FUN{@FA{vertex}.getPropertyKeys()}
///
/// Returns all propety names a @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-property-keys
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getPropertyKeys = function () {
  return this._properties.propertyKeys;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all properties of a vertex
///
/// @FUN{@FA{vertex}.properties()}
///
/// Returns all properties and their values of a @FA{vertex}
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-properties
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.properties = function () {
  return this._properties._shallowCopy;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////


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
/// @brief adds an edge to the graph
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{id})}
///
/// Creates a new edge from @FA{out} to @FA{in} and returns the edge object. The
/// identifier @FA{id} must be a unique identifier or null.
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{id}, @FA{label})}
///
/// Creates a new edge from @FA{out} to @FA{in} with @FA{label} and returns the
/// edge object.
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{id}, @FA{data})}
///
/// Creates a new edge and returns the edge object. The edge contains the
/// properties defined in @FA{data}.
///
/// @FUN{@FA{graph}.addEdge(@FA{out}, @FA{in}, @FA{id}, @FA{label}, @FA{data})}
///
/// Creates a new edge and returns the edge object. The edge has the
/// label @FA{label} and contains the properties defined in @FA{data}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-add-edge
///
/// @verbinclude graph-graph-add-edge2
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addEdge = function (out_vertex, in_vertex, id, label, data, waitForSync) {
  return this._saveEdge(id, out_vertex, in_vertex, this._prepareEdgeData(data, label), waitForSync);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a vertex to the graph
///
/// @FUN{@FA{graph}.addVertex(@FA{id})}
///
/// Creates a new vertex and returns the vertex object. The identifier
/// @FA{id} must be a unique identifier or null.
///
/// @FUN{@FA{graph}.addVertex(@FA{id}, @FA{data})}
///
/// Creates a new vertex and returns the vertex object. The vertex contains
/// the properties defined in @FA{data}.
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
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addVertex = function (id, data, waitForSync) {
  return this._saveVertex(id, this._prepareVertexData(data), waitForSync);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief graph printing
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._PRINT = function (context) {
  context.output += "Graph(\"" + this._properties._key + "\")";
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.Edge = Edge;
exports.Graph = Graph;
exports.Vertex = Vertex;
exports.GraphArray = GraphArray;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
