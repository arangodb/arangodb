module.define("org/arangodb/graph", function(exports, module) {
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
  arangosh = require("org/arangodb/arangosh"),
  is = require("org/arangodb/is"),
  ArangoQueryCursor = require("org/arangodb/arango-query-cursor").ArangoQueryCursor,
  common = require("org/arangodb/graph-common"),
  Edge = common.Edge,
  Graph = common.Graph,
  Vertex = common.Vertex,
  GraphArray = common.GraphArray,
  Iterator,
  GraphAPI;

Iterator = function (graph, cursor, MyPrototype, stringRepresentation) {
  this.next = function next() {
    if (cursor.hasNext()) {
      return new MyPrototype(graph, cursor.next());
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

GraphAPI = {
  send: function (method, graphKey, path, data) {
    var results = arangodb.arango[method]("/_api/graph/" +
      encodeURIComponent(graphKey) +
      path,
      JSON.stringify(data));

    arangosh.checkRequestResult(results);
    return results;
  },

  sendWithoutData: function (method, graphKey, path) {
    var results = arangodb.arango[method]("/_api/graph/" +
      encodeURIComponent(graphKey) +
      path);

    arangosh.checkRequestResult(results);
    return results;
  },

  // Graph
  getGraph: function (graphKey) {
    return GraphAPI.sendWithoutData("GET", graphKey, "");
  },

  postGraph: function (data) {
    var results = arangodb.arango.POST("/_api/graph", JSON.stringify(data));
    arangosh.checkRequestResult(results);
    return results;
  },

  deleteGraph: function (graphKey) {
    return GraphAPI.sendWithoutData("DELETE", graphKey, "");
  },

  // Vertex
  getVertex: function (graphKey, vertexKey) {
    var results;

    try {
      results = GraphAPI.sendWithoutData("GET",
        graphKey,
        "/vertex/" +
        encodeURIComponent(vertexKey));
    } catch(e) {
      if (e instanceof arangodb.ArangoError && e.code === 404) {
        results = null;
      } else {
        throw(e);
      }
    }

    return results;
  },

  putVertex: function (graphKey, vertexKey, data) {
    return GraphAPI.send("PUT",
      graphKey,
      "/vertex/" + encodeURIComponent(vertexKey),
      data);
  },

  postVertex: function(graphKey, data) {
    return GraphAPI.send("POST",
      graphKey,
      "/vertex",
      data);
  },

  deleteVertex: function (graphKey, vertexKey) {
    GraphAPI.sendWithoutData("DELETE",
      graphKey,
      "/vertex/" + encodeURIComponent(vertexKey));
  },

  // Edge
  getEdge: function (graphKey, edgeKey) {
    var results;

    try {
      results = GraphAPI.sendWithoutData("GET",
        graphKey,
        "/edge/" + encodeURIComponent(edgeKey));
    } catch(e) {
      if (e instanceof arangodb.ArangoError && e.code === 404) {
        results = null;
      } else {
        throw(e);
      }
    }

    return results;
  },

  putEdge: function (graphKey, edgeKey, data) {
    return GraphAPI.send("PUT",
      graphKey,
      "/edge/" + encodeURIComponent(edgeKey),
      data);
  },

  postEdge: function(graphKey, data) {
    return GraphAPI.send("POST",
      graphKey,
      "/edge",
      data);
  },

  deleteEdge: function (graphKey, edgeKey) {
    GraphAPI.sendWithoutData("DELETE",
      graphKey,
      "/edge/" + encodeURIComponent(edgeKey));
  },

  // Vertices
  getVertices: function (database, graphKey, data) {
    var results = GraphAPI.send("POST",
      graphKey,
      "/vertices",
      data);

    return new ArangoQueryCursor(database, results);
  },

  // Edges
  getEdges: function (database, graphKey, data) {
    var results = GraphAPI.send("POST",
      graphKey,
      "/edges",
      data);

    return new ArangoQueryCursor(database, results);
  },

  postEdges: function (database, graphKey, edge, data) {
    var results = GraphAPI.send("POST",
      graphKey,
      "/edges/" + encodeURIComponent(edge._properties._key),
      data);

    return new ArangoQueryCursor(database, results);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                       module "org/arangodb/graph"
// -----------------------------------------------------------------------------

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
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getInVertex = function () {
  return this._graph.getVertex(this._properties._to);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the from vertex
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getOutVertex = function () {
  return this._graph.getVertex(this._properties._from);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the other vertex
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
/// @brief changes a property of an edge
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.setProperty = function (name, value) {
  var results,
    update = this._properties;

  update[name] = value;

  results = GraphAPI.putEdge(this._graph._properties._key, this._properties._key, update);

  this._properties = results.edge;

  return name;
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
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.edges = function (direction, labels) {
  var edge,
    edges = new GraphArray(),
    cursor;

  cursor = GraphAPI.postEdges(this._graph._vertices._database, this._graph._properties._key, this, {
    filter : { direction : direction, labels: labels }
  });

  while (cursor.hasNext()) {
    edge = new Edge(this._graph, cursor.next());
    edges.push(edge);
  }

  return edges;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges with given label
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getInEdges = function () {
  var labels = Array.prototype.slice.call(arguments);
  return this.edges("in", labels);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges with given label
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getOutEdges = function () {
  var labels = Array.prototype.slice.call(arguments);
  return this.edges("out", labels);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief in- or outbound edges with given label
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getEdges = function () {
  var labels = Array.prototype.slice.call(arguments);
  return this.edges("any", labels);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inbound = function () {
  return this.getInEdges();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outbound = function () {
  return this.getOutEdges();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of a vertex
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.setProperty = function (name, value) {
  var results,
    update = this._properties;

  update[name] = value;

  results = GraphAPI.putVertex(this._graph._properties._key, this._properties._key, update);

  this._properties = results.vertex;

  return name;
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
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.initialize = function (name, vertices, edges) {
  var results;

  if (vertices === undefined && edges === undefined) {
    results = GraphAPI.getGraph(name);
  } else {
    results = GraphAPI.postGraph({
      _key: name,
      vertices: vertices,
      edges: edges
    });
  }

  this._properties = results.graph;
  this._vertices = arangodb.db._collection(this._properties.vertices);
  this._edges = arangodb.db._collection(this._properties.edges);

  return this;
};

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
////////////////////////////////////////////////////////////////////////////////
Graph.prototype.drop = function () {
  GraphAPI.deleteGraph(this._properties._key);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an edge to the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._saveEdge = function(id, out_vertex, in_vertex, params) {
  var results;

  params._key = id;
  params._from = out_vertex._properties._key;
  params._to = in_vertex._properties._key;

  results = GraphAPI.postEdge(this._properties._key, params);
  return new Edge(this, results.edge);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a vertex to the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._saveVertex = function (id, params) {
  var results;

  if (is.existy(id)) {
    params._key = id;
  }

  results = GraphAPI.postVertex(this._properties._key, params);
  return new Vertex(this, results.vertex);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a vertex given its id
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertex = function (id) {
  var results = GraphAPI.getVertex(this._properties._key, id);

  if (results === null) {
    return null;
  }

  return new Vertex(this, results.vertex);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all vertices
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertices = function () {
  var cursor = GraphAPI.getVertices(this._vertices._database, this._properties._key, {});
  return new Iterator(this, cursor, Vertex, "[vertex iterator]");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an edge given its id
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdge = function (id) {
  var results = GraphAPI.getEdge(this._properties._key, id);

  if (results === null) {
    return null;
  }

  return new Edge(this, results.edge);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all edges
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdges = function () {
  var cursor = GraphAPI.getEdges(this._vertices._database, this._properties._key, {});
  return new Iterator(this, cursor, Edge, "[edge iterator]");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a vertex and all in- or out-bound edges
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeVertex = function (vertex) {
  GraphAPI.deleteVertex(this._properties._key, vertex._properties._key);
  vertex._properties = undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeEdge = function (edge) {
  GraphAPI.deleteEdge(this._properties._key, edge._properties._key);
  edge._properties = undefined;
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
});
