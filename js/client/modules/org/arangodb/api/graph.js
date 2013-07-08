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

var GraphAPI,
  arangodb = require("org/arangodb"),
  arangosh = require("org/arangodb/arangosh"),
  ArangoQueryCursor = require("org/arangodb/arango-query-cursor").ArangoQueryCursor;

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

exports.GraphAPI = GraphAPI;
