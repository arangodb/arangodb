/*jshint strict: false, unused: false */

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
  db = arangodb.db,
  ArangoCollection = arangodb.ArangoCollection,
  common = require("org/arangodb/graph-common"),
  newGraph = require("org/arangodb/general-graph"),
  Edge = common.Edge,
  Graph = common.Graph,
  Vertex = common.Vertex,
  GraphArray = common.GraphArray,
  Iterator = common.Iterator;

// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/graph-blueprint"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create a collection by name
////////////////////////////////////////////////////////////////////////////////

var findOrCreateCollectionByName = function (name) {
  var col = db._collection(name);

  if (col === null) {
    col = db._create(name);
  } else if (!(col instanceof ArangoCollection) || col.type() !== ArangoCollection.TYPE_DOCUMENT) {
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
  } else if (!(col instanceof ArangoCollection) || col.type() !== ArangoCollection.TYPE_EDGE) {
    throw "<" + name + "> must be an edge collection";
  }

  if (col === null) {
    throw "collection '" + name + "' has vanished";
  }

  return col;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                              Edge
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock edgeSetProperty
///
/// `edge.setProperty(name, value)`
///
/// Changes or sets the property *name* an *edges* to *value*.
///
/// @EXAMPLES
///
/// @verbinclude graph-edge-set-property
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.setProperty = function (name, value) {
  var shallow = this._properties._shallowCopy;
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

// -----------------------------------------------------------------------------
// --SECTION--                                                            Vertex
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block vertexEdges
///
/// `vertex.edges()`
///
/// Returns a list of in- or outbound edges of the *vertex*.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-edges
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.edges = function () {
  var graph = this._graph;

  return graph._edges.edges(this._properties._id).map(function (result) {
    return graph.constructEdge(result);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block vertexGetInEdges
///
///`vertex.getInEdges(label, ...)`
///
/// Returns a list of inbound edges of the *vertex* with given label(s).
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-in-edges
/// @end Docu Block
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
/// @start Docu Block vertexGetOutEdges
///
/// `vertex.getOutEdges(label, ...)`
///
/// Returns a list of outbound edges of the *vertex* with given label(s).
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-out-edges
/// @end Docu Block
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
/// @start Docu Block vertexGetEdges
///
/// `vertex.getEdges(label, ...)`
///
/// Returns a list of in- or outbound edges of the *vertex* with given
/// label(s).
/// @end Docu Block
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
/// @start Docu Block vertexInbound
///
/// `vertex.inbound()`
///
/// Returns a list of inbound edges of the *vertex*.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-inbound
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inbound = function () {
  var graph = this._graph;

  return graph._edges.inEdges(this._properties._id).map(function (result) {
    return graph.constructEdge(result);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block vertexOutbound
///
/// `vertex.outbound()`
///
/// Returns a list of outbound edges of the *vertex*.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-outbound
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outbound = function () {
  var graph = this._graph;

  return graph._edges.outEdges(this._properties._id).map(function (result) {
    return graph.constructEdge(result);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block vertexSetProperty
///
/// `vertex.setProperty(name, value)`
///
/// Changes or sets the property *name* a *vertex* to *value*.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-set-property
/// @end Docu BLock
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.setProperty = function (name, value) {
  var shallow = this._properties._shallowCopy;
  var id;

  shallow[name] = value;

  // TODO use "update" if this becomes available
  id = this._graph._vertices.replace(this._properties, shallow);
  this._properties = this._graph._vertices.document(id);

  return value;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                             Graph
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block graphConstruct
///
/// `Graph(name, vertices, edges)`
///
/// Constructs a new graph object using the collection *vertices* for all
/// vertices and the collection *edges* for all edges. Note that it is
/// possible to construct two graphs with the same vertex set, but different
/// edge sets.
///
/// `Graph(name)`
///
/// Returns a known graph.
///
/// @EXAMPLES
///
/// @verbinclude graph-constructor
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.initialize = function (name, vertices, edges, waitForSync) {
  this._name = name;
  var gdb = db._collection("_graphs");
  var graphProperties;
  var graphPropertiesId;

  if (gdb === null) {
    throw "_graphs collection does not exist.";
  }

  if (typeof name !== "string" || name === "") {
    throw "<name> must be a string";
  }

  // convert collection objects to collection names
  if (typeof vertices === 'object' && typeof vertices.name === 'function') {
    vertices = vertices.name();
  }

  if (typeof edges === 'object' && typeof edges.name === 'function') {
    edges = edges.name();
  }

  // find an existing graph by name
  if (vertices === undefined && edges === undefined) {

    try {
      graphProperties = gdb.document(name);
    }
    catch (e) {
      throw "no graph named '" + name + "' found";
    }

    if (graphProperties === null) {
      throw "no graph named '" + name + "' found";
    }

    //check if graph can be loaded by this deprecated module
    var newGraphError = "Graph can not be loaded, "
      + "because more than 1 vertex collection is defined. "
      + "Please use the new graph module";
    var edgeDefinitions = db._graphs.document(name).edgeDefinitions;
    if (edgeDefinitions.length === 0) {
      throw newGraphError;
    }
    if (edgeDefinitions.length > 1) {
      throw newGraphError;
    } else if (edgeDefinitions.length === 1) {
      var from = edgeDefinitions[0].from;
      var to = edgeDefinitions[0].to;
      if (from.length !== 1 || to.length !== 1 || from[0] !== to[0]) {
        throw newGraphError;
      }
    }

    vertices = db._collection(edgeDefinitions[0].from[0]);

    if (vertices === null) {
      throw "vertex collection '" + edgeDefinitions[0].from[0] + "' has vanished";
    }

    edges = db._collection(edgeDefinitions[0].collection);

    if (edges === null) {
      throw "edge collection '" + edgeDefinitions[0].collection + "' has vanished";
    }
  }

  // sanity check for vertices
  else if (typeof vertices !== "string" || vertices === "") {
    throw "<vertices> must be a string or null";
  }

  // sanity check for edges
  else if (typeof edges !== "string" || edges === "") {
    throw "<edges> must be a string or null";
  }

  // create a new graph or get an existing graph
  else {
    try {
      graphProperties = gdb.document(name);
    }
    catch (e1) {
      graphProperties = null;
    }

    // graph doesn't exist yet, create it
    if (graphProperties === null) {

      // check if know that graph
      graphProperties = gdb.firstExample(
        'edgeDefintions', [{"collection": edges, "from" :[vertices], "to": [vertices]}]
      );

      if (graphProperties === null) {

        // check if edge is used in a graph
        gdb.toArray().forEach(
          function(singleGraph) {
            var sGEDs = singleGraph.edgeDefinitions;
            sGEDs.forEach(
              function(sGED) {
                if (sGED.collection === edges) {
                  graphProperties = "";
                }
              }
            );
          }
        );

        if (graphProperties === null) {
          findOrCreateCollectionByName(vertices);
          findOrCreateEdgeCollectionByName(edges);

          var newEdgeDefinition = [{"collection": edges, "from" :[vertices], "to": [vertices]}];

          graphPropertiesId = gdb.save(
            {
              'edgeDefinitions' : newEdgeDefinition,
              '_key' : name
            },
            waitForSync
          );

          graphProperties = gdb.document(graphPropertiesId._key);
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
      if (graphProperties.edgeDefinitions[0].from[0] !== vertices
        || graphProperties.edgeDefinitions[0].to[0] !== vertices
        || graphProperties.edgeDefinitions[0].collection !== edges) {
        throw "graph with that name already exists!";
      }
    }

    vertices = db._collection(graphProperties.edgeDefinitions[0].from[0]);
    edges = db._collection(graphProperties.edgeDefinitions[0].collection);
  }
  this._properties = graphProperties;

  // and store the collections
  this._gdb = gdb;
  this._vertices = vertices;
  this._edges = edges;

  // and dictionary for vertices and edges
  this._verticesCache = {};
  this._edgesCache = {};

  // and store the caches
  this.predecessors = {};
  this.distances = {};
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block JSF_graph_getAll
///
/// `graph.getAll()`
///
/// Returns all available graphs.
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Graph.getAll = function getAllGraphs () {
  var gdb = db._collection("_graphs"),
    graphs = [ ];

  gdb.toArray().forEach(function(doc) {
    try {
      var g = new Graph(doc._key);

      if (g._properties !== null) {
        graphs.push(g._properties);
      }
    }
    catch (err) {
      // if there's a problem, we just skip this graph
    }
  });

  return graphs;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief static drop function
////////////////////////////////////////////////////////////////////////////////

Graph.drop = function (name, waitForSync) {
  var gdb = db._collection("_graphs");
  var exists = gdb.exists(name);

  try {
    var obj = new Graph(name);
    return obj.drop(waitForSync);
  }
  catch (err) {
    if (exists) {
      // if the graph exists but cannot be deleted because one of the underlying
      // collections is missing, delete from _graphs "manually"
      gdb.remove(name, true, waitForSync);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block graphDrop
///
/// `graph.drop(waitForSync)`
///
/// Drops the graph, the vertices, and the edges. Handle with care.
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.drop = function (waitForSync) {
  newGraph._drop(this._name, true);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an edge to the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._saveEdge = function(id, out_vertex_id, in_vertex_id, shallow, waitForSync) {
  this.emptyCachedPredecessors();

  if (id !== undefined && id !== null) {
    shallow._key = String(id);
  }

  var ref = this._edges.save(out_vertex_id,
                             in_vertex_id,
                             shallow,
                             waitForSync);

  return this.constructEdge(ref._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a vertex to the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._saveVertex = function (id, shallow, waitForSync) {
  var ref;

  if (is.existy(id)) {
    shallow._key = String(id);
  }

  ref = this._vertices.save(shallow, waitForSync);

  return this.constructVertex(ref._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a vertex to the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._replaceVertex = function (vertex_id, data) {
  this._vertices.replace(vertex_id, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces an edge in the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._replaceEdge = function (edge_id, data) {
  this._edges.replace(edge_id, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block graphGetVertex
///
/// `graph.getVertex(id)`
///
/// Returns the vertex identified by *id* or *null*.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-vertex
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertex = function (id) {
  try {
    return this.constructVertex(id);
  }
  catch (e) {
    return null;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block GraphGetVertices
///
/// `graph.getVertices()`
///
/// Returns an iterator for all vertices of the graph. The iterator supports the
/// methods *hasNext* and *next*.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-vertices
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertices = function () {
  var all = this._vertices.all(),
    graph = this,
    wrapper = function(object) {
      return graph.constructVertex(object);
    };

  return new Iterator(wrapper, all, "[edge iterator]");
};

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block graphGetEdge
///
/// `graph.getEdge(id)`
///
/// Returns the edge identified by *id* or *null*.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-edge
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdge = function (id) {
  var ref, edge;

  try {
    ref = this._edges.document(id);
  } catch (e) {
    ref = null;
  }

  if (ref !== null) {
    edge = this.constructEdge(ref);
  } else {
    try {
      edge = this.constructEdge(id);
    } catch (e1) {
      edge = null;
    }
  }

  return edge;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBLock graphGetEdges
///
/// `graph.getEdges()`
///
/// Returns an iterator for all edges of the graph. The iterator supports the
/// methods *hasNext* and *next*.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-get-edges
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdges = function () {
  var all = this._edges.all(),
  graph = this,
  wrapper = function(object) {
    return graph.constructEdge(object);
  };
  return new Iterator(wrapper, all, "[edge iterator]");
};

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block graphRemoveVertex
///
/// `graph.removeVertex(vertex, waitForSync)`
///
/// Deletes the *vertex* and all its edges.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-remove-vertex
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeVertex = function (vertex, waitForSync) {
  var result, graph = this;

  this.emptyCachedPredecessors();

  if (vertex._properties) {
    result = this._vertices.remove(vertex._properties, true, waitForSync);

    if (!result) {
      throw "cannot delete vertex";
    }

    vertex.edges().forEach(function (edge) {
      graph.removeEdge(edge, waitForSync);
    });

    vertex._properties = undefined;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @start Docu Block graphRemoveEdge
///
/// `graph.removeEdge(vertex, waitForSync)`
///
/// Deletes the *edge*. Note that the in and out vertices are left untouched.
///
/// @EXAMPLES
///
/// @verbinclude graph-graph-remove-edge
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeEdge = function (edge, waitForSync) {
  var result;

  this.emptyCachedPredecessors();

  if (edge._properties) {
    result = this._edges.remove(edge._properties, true, waitForSync);

    if (! result) {
      throw "cannot delete edge";
    }

    this._edgesCache[edge._properties._id] = undefined;
    edge._properties = undefined;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.Edge = Edge;
exports.Graph = Graph;
exports.Vertex = Vertex;
exports.GraphArray = GraphArray;

require("org/arangodb/graph/algorithms-common");


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
