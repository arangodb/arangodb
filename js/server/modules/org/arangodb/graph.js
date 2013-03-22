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

var arangodb = require("org/arangodb");

var db = arangodb.db;
var ArangoCollection = arangodb.ArangoCollection;

// -----------------------------------------------------------------------------
// --SECTION--                                       module "org/arangodb/graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create a collection by name
////////////////////////////////////////////////////////////////////////////////

var findOrCreateCollectionByName = function (name) {
  var col = db._collection(name);

  if (col === null) {
    col = db._create(name);
  } 
  else if (!(col instanceof ArangoCollection) || col.type() !== ArangoCollection.TYPE_DOCUMENT) {
    throw "<" + name + "> must be a document collection";
  }

  if (col === null) {
    throw "collection '" + name + "' has vanished";
  }

  return col;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create an edge collection by name
////////////////////////////////////////////////////////////////////////////////

var findOrCreateEdgeCollectionByName = function (name) {
  var col = db._collection(name);

  if (col === null) {
    col = db._createEdgeCollection(name);
  }
  else if (!(col instanceof ArangoCollection) || col.type() !== ArangoCollection.TYPE_EDGE) {
    throw "<" + name + "> must be an edge collection";
  }

  if (col === null) {
    throw "collection '" + name + "' has vanished";
  }

  return col;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                              Edge
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new edge object
////////////////////////////////////////////////////////////////////////////////

function Edge (graph, properties) {
  this._graph = graph;
  this._properties = properties;
}

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
  return this._graph.constructVertex(this._properties._to);
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
  return this._graph.constructVertex(this._properties._from);
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
    return this._graph.constructVertex(this._properties._from);
  }

  if (vertex._properties._id === this._properties._from) {
    return this._graph.constructVertex(this._properties._to);
  }

  return null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of an edge
///
/// @FUN{@FA{edge}.setProperty(@FA{name}, @FA{value})}
///
/// Changes or sets the property @FA{name} an @FA{edges} to @FA{value}.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-set-property
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.setProperty = function (name, value) {
  var shallow = this._properties.shallowCopy;
  var id;

  // Could potentially change the weight of edges
  this._graph.emptyCachedPredecessors();

  shallow.$label = this._properties.$label;
  shallow[name] = value;

  // TODO use "update" if this becomes available
  id = this._graph._edges.replace(this._properties, shallow);
  this._properties = this._graph._edges.document(id);

  return value;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            Vertex
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new vertex object
////////////////////////////////////////////////////////////////////////////////

function Vertex (graph, properties) {
  this._graph = graph;
  this._properties = properties;
}

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
/// @brief inbound and outbound edges
///
/// @FUN{@FA{vertex}.edges()}
///
/// Returns a list of in- or outbound edges of the @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.edges = function () {
  var graph = this._graph;

  return graph._edges.edges(this._properties._id).map(function (result) {
    return graph.constructEdge(result);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges with given label
///
/// @FUN{@FA{vertex}.getInEdges(@FA{label}, ...)}
///
/// Returns a list of inbound edges of the @FA{vertex} with given label(s).
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-in-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getInEdges = function () {
  var labels = Array.prototype.slice.call(arguments);
  var result = this.inbound();

  if (labels.length > 0) {
    result = result.filter(function (edge) {
      return (labels.lastIndexOf(edge.getLabel()) > -1);
    });
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges with given label
///
/// @FUN{@FA{vertex}.getOutEdges(@FA{label}, ...)}
///
/// Returns a list of outbound edges of the @FA{vertex} with given label(s).
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-out-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getOutEdges = function () {
  var labels = Array.prototype.slice.call(arguments);
  var result = this.outbound();

  if (labels.length > 0) {
    result = result.filter(function (edge) {
      return (labels.lastIndexOf(edge.getLabel()) > -1);
    });
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief in- or outbound edges with given label
///
/// @FUN{@FA{vertex}.getEdges(@FA{label}, ...)}
///
/// Returns a list of in- or outbound edges of the @FA{vertex} with given
/// label(s).
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getEdges = function () {
  var labels = Array.prototype.slice.call(arguments);
  var result = this.edges();

  if (labels.length > 0) {
    result = result.filter(function (edge) {
      return (labels.lastIndexOf(edge.getLabel()) > -1);
    });
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges
///
/// @FUN{@FA{vertex}.inbound()}
///
/// Returns a list of inbound edges of the @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-inbound
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inbound = function () {
  var graph = this._graph;

  return graph._edges.inEdges(this._properties._id).map(function (result) {
    return graph.constructEdge(result);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges
///
/// @FUN{@FA{vertex}.outbound()}
///
/// Returns a list of outbound edges of the @FA{vertex}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-outbound
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outbound = function () {
  var graph = this._graph;

  return graph._edges.outEdges(this._properties._id).map(function (result) {
    return graph.constructEdge(result);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of a vertex
///
/// @FUN{@FA{vertex}.setProperty(@FA{name}, @FA{value})}
///
/// Changes or sets the property @FA{name} a @FA{vertex} to @FA{value}.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-set-property
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.setProperty = function (name, value) {
  var shallow = this._properties.shallowCopy;
  var id;

  shallow[name] = value;

  // TODO use "update" if this becomes available
  id = this._graph._vertices.replace(this._properties, shallow);
  this._properties = this._graph._vertices.document(id);

  return value;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of edges
///
/// @FUN{@FA{vertex}.degree()}
///
/// Returns the number of edges of the @FA{vertex}.
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.degree = function () {
  return this._graph._edges.edges(this._properties._id).length;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of in-edges
///
/// @FUN{@FA{vertex}.inDegree()}
///
/// Returns the number of inbound edges of the @FA{vertex}.
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inDegree = function () {
  return this._graph._edges.inEdges(this._properties._id).length;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of out-edges
///
/// @FUN{@FA{vertex}.outDegree()}
///
/// Returns the number of outbound edges of the @FA{vertex}.
///
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outDegree = function () {
  return this._graph._edges.outEdges(this._properties._id).length;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             Graph
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoGraph
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new graph object
///
/// @FUN{Graph(@FA{name}, @FA{vertices}, @FA{edges})}
///
/// Constructs a new graph object using the collection @FA{vertices} for all
/// vertices and the collection @FA{edges} for all edges. Note that it is
/// possible to construct two graphs with the same vertex set, but different
/// edge sets.
///
/// @FUN{Graph(@FA{name})}
///
/// Returns a known graph.
///
/// @EXAMPLES
///
/// @verbinclude graph-constructor
////////////////////////////////////////////////////////////////////////////////

function Graph (name, vertices, edges, waitForSync) {
  var gdb = db._collection("_graphs");
  var graphProperties;
  var graphPropertiesId;

  if (gdb === null) {
    throw "_graphs collection does not exist.";
  }

  if (typeof name !== "string" || name === "") {
    throw "<name> must be a string";
  }

  if (vertices === undefined && edges === undefined) {

    // Find an existing graph
    try {
      graphProperties = gdb.document(name);
    }
    catch (e) {
      throw "no graph named '" + name + "' found";
    }

    if (graphProperties === null) {
      throw "no graph named '" + name + "' found";
    }

    vertices = db._collection(graphProperties.vertices);

    if (vertices === null) {
      throw "vertex collection '" + graphProperties.vertices + "' has vanished";
    }

    edges = db._collection(graphProperties.edges);

    if (edges === null) {
      throw "edge collection '" + graphProperties.edges + "' has vanished";
    }
  }
  else if (typeof vertices !== "string" || vertices === "") {
    throw "<vertices> must be a string or null";
  }
  else if (typeof edges !== "string" || edges === "") {
    throw "<edges> must be a string or null";
  }
  else {

    // Create a new graph or get an existing graph
    vertices = findOrCreateCollectionByName(vertices);
    edges = findOrCreateEdgeCollectionByName(edges);

    try {
      graphProperties = gdb.document(name);
    }
    catch (e1) {
      graphProperties = null;
    }

    // Graph doesn't exist yet
    if (graphProperties === null) {

      // check if know that graph
      graphProperties = gdb.firstExample(
        'vertices', vertices.name(),
        'edges', edges.name()
      );

      if (graphProperties === null) {
        
         // check if edge is used in a graph
        graphProperties = gdb.firstExample('edges', edges.name());

        if (graphProperties === null) {      
          graphPropertiesId = gdb.save({
            'vertices' : vertices.name(),
            'edges' : edges.name(),
            '_key' : name
          }, waitForSync);

          graphProperties = gdb.document(graphPropertiesId);
        }
        else {
          throw "edge collection already used";
        }         
      }
      else {
        throw "found graph but has different <name>";
      }
    }
    else {
      throw "graph with that name already exists";
      //if (graphProperties.vertices !== vertices.name()) {
      //  throw "found graph but has different <vertices>";
      //}

      //if (graphProperties.edges !== edges.name()) {
      //  throw "found graph but has different <edges>";
      //}
    }
  }

  this._properties = graphProperties;

  // and store the collections
  this._gdb = gdb;
  this._vertices = vertices;
  this._edges = edges;

  // and dictionary for vertices and edges
  this._verticesCache = {};
  this._edgesCache = {};

  // and store the cashes
  this.predecessors = {};
  this.distances = {};
}

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
/// @brief drops the graph, the vertices, and the edges
///
/// @FUN{@FA{graph}.drop(@FA{waitForSync})}
///
/// Drops the graph, the vertices, and the edges. Handle with care.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.drop = function (waitForSync) {
  var gdb = db._collection("_graphs");

  gdb.remove(this._properties, true, waitForSync);

  this._vertices.drop();
  this._edges.drop();
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
  var ref;
  var shallow;

  this.emptyCachedPredecessors();

  if (typeof label === 'object') {
    data = label;
    label = undefined;
  }

  if (label === undefined) {
    label = null;
  }

  if (data === null || typeof data !== "object") {
    shallow = {};
  }
  else {
    shallow = data.shallowCopy || {};
  }

  if (id !== undefined && id !== null) {
    shallow._key = String(id);
  }

  shallow.$label = label || null;

  ref = this._edges.save(out_vertex._properties._id, 
                         in_vertex._properties._id, shallow, waitForSync);

  return this.constructEdge(ref._id);
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
  var ref;
  var shallow;

  if (data === null || typeof data !== "object") {
    shallow = {};
  }
  else {
    shallow = data.shallowCopy || {};
  }

  if (id !== undefined && id !== null) {
    shallow._key = String(id);
  }

  ref = this._vertices.save(shallow, waitForSync);

  return this.constructVertex(ref._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a vertex given its id
///
/// @FUN{@FA{graph}.getVertex(@FA{id})}
///
/// Returns the vertex identified by @FA{id} or @LIT{null}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-vertex
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertex = function (id) {
  var ref;
  var vertex;

  try {
    ref = this._vertices.document(id);
  }
  catch (e) {
    ref = null;
  }

  if (ref !== null) {
    vertex = this.constructVertex(ref);
  }
  else {
    try {
      vertex = this.constructVertex(id);
    }
    catch (e1) {
      vertex = null;
    }
  }

  return vertex;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all vertices
///
/// @FUN{@FA{graph}.getVertices()}
///
/// Returns an iterator for all vertices of the graph. The iterator supports the
/// methods @FN{hasNext} and @FN{next}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-vertices
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertices = function () {
  var that = this;
  var all = this._vertices.all();
  var v;
  var Iterator;

  Iterator = function () {
    this.next = function next() {
      if (all.hasNext()) {
        return that.constructVertex(all.next());
      }

      return undefined;
    };

    this.hasNext = function hasNext() {
      return all.hasNext();
    };

    this._PRINT = function (seen, path, names) {
      // Ignores the standard arguments
      seen = path = names = null;

      arangodb.output("[vertex iterator]");
    };
  };

  return new Iterator();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an edge given its id
///
/// @FUN{@FA{graph}.getEdge(@FA{id})}
///
/// Returns the edge identified by @FA{id} or @LIT{null}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdge = function (id) {
  var ref;
  var edge;

  try {
    ref = this._edges.document(id);
  }
  catch (e) {
    ref = null;
  }

  if (ref !== null) {
    edge = this.constructEdge(ref);
  }
  else {
    try {
      edge = this.constructEdge(id);
    }
    catch (e1) {
      edge = null;
    }
  }

  return edge;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all edges
///
/// @FUN{@FA{graph}.getEdges()}
///
/// Returns an iterator for all edges of the graph. The iterator supports the
/// methods @FN{hasNext} and @FN{next}.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-edges
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdges = function () {
  var that = this;
  var all = this._edges.all();
  var v;
  var Iterator;

  Iterator = function () {
    this.next = function next() {
      if (all.hasNext()) {
        return that.constructEdge(all.next());
      }

      return undefined;
    };

    this.hasNext = function hasNext() {
      return all.hasNext();
    };

    this._PRINT = function (seen, path, names) {
      // Ignores the standard arguments
      seen = path = names = null;

      arangodb.output("[edge iterator]");
    };
  };

  return new Iterator();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a vertex and all in- or out-bound edges
///
/// @FUN{@FA{graph}.removeVertex(@FA{vertex},@FA{waitForSync})}
///
/// Deletes the @FA{vertex} and all its edges.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-remove-vertex
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeVertex = function (vertex, waitForSync) {
  var result;
  var graph = this;

  this.emptyCachedPredecessors();

  if (vertex._properties) {
    result = this._vertices.remove(vertex._properties, true, waitForSync);

    if (! result) {
      throw "cannot delete vertex";
    }

    vertex.edges().forEach(function (edge) {
      graph.removeEdge(edge, waitForSync);
    });

    vertex._properties = undefined;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an edge
///
/// @FUN{@FA{graph}.removeEdge(@FA{vertex},@FA{waitForSync})}
///
/// Deletes the @FA{edge}. Note that the in and out vertices are left untouched.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-remove-edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeEdge = function (edge, waitForSync) {
  var result;

  this.emptyCachedPredecessors();

  if (edge._properties) {
    result = this._edges.remove(edge._properties, true, waitForSync);

    if (! result) {
      throw "cannot delete edge";
    }

    edge._properties = undefined;
  }
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
  var id;

  if (typeof data === "string") {
    id = data;
  }
  else {
    id = data._id;
  }

  var vertex = this._verticesCache[id];

  if (vertex === undefined) {
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
  var id;

  if (typeof data === "string") {
    id = data;
  }
  else {
    id = data._id;
  }

  var edge = this._edgesCache[id];

  if (edge === undefined) {
    var properties = this._edges.document(id);

    if (! properties) {
      throw "accessing a deleted edge";
    }

    this._edgesCache[id] = edge = new Edge(this, properties);
  }

  return edge;
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

require("org/arangodb/graph-common");
require("org/arangodb/graph/algorithms");

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
