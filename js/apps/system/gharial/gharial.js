/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, unparam: true */
/*global require, applicationContext*/

////////////////////////////////////////////////////////////////////////////////
/// @brief A Foxx.Controller to show all Foxx Applications
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
(function() {
  "use strict";

  var FoxxController = require("org/arangodb/foxx").Controller,
    controller = new FoxxController(applicationContext),
    ArangoError = require("org/arangodb").ArangoError,
    actions = require("org/arangodb/actions"),
    Model = require("org/arangodb/foxx").Model,
    Graph = require("org/arangodb/general-graph"),
    toId = function(c, k) {
      return c + "/" + k;
    },
    _ = require("underscore"),
    setResponse = function (res, name, body, code) {
      var obj = {};
      obj.error = false;
      obj.code = code || actions.HTTP_OK;
      if (name !== undefined && body !== undefined) {
        obj[name] = body;
        if (body._rev) {
          res.set("etag", body._rev);
        }
      }
      res.json(obj);
      if (code) {
        res.status(code);
      }
    };

  /** Create a new vertex.
   *
   * Stores a new vertex with the information contained
   * within the body into the given collection.
   */
  controller.post("/:graph/vertex/:collection", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var body = req.params("vertex");
    var g = Graph._graph(name);
    setResponse(res, "vertex", g[collection].save(body.forDB()));
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the vertex collection."
  })
  .bodyParam("vertex", "The document to be stored", Model);

  /** Load a vertex.
   *
   * Loads a vertex with the given id if it is contained
   * within your graph.
   */
  controller.get("/:graph/vertex/:collection/:key", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var key = req.params("key");
    var id = toId(collection, key);
    var g = Graph._graph(name);
    setResponse(res, "vertex", g[collection].document(id));
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the vertex collection."
  })
  .pathParam("key", {
    type: "string",
    description: "_key attribute of one specific vertex."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The vertex does not exist.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

  /** Replace a vertex.
   *
   * Replaces a vertex with the given id by the content in the body.
   * This will only run successfully if the vertex is contained
   * within the graph.
   */
  controller.put("/:graph/vertex/:collection/:key", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var key = req.params("key");
    var id = toId(collection, key);
    var body = req.params("vertex");
    var g = Graph._graph(name);
    setResponse(res, "vertex", g[collection].replace(id, body.forDB()));
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the vertex collection."
  })
  .pathParam("key", {
    type: "string",
    description: "_key attribute of one specific vertex."
  })
  .bodyParam("vertex", "The document to be stored", Model)
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The vertex does not exist.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

  /** Update a vertex.
   *
   * Updates a vertex with the given id by adding the content in the body.
   * This will only run successfully if the vertex is contained
   * within the graph.
   */
  controller.patch("/:graph/vertex/:collection/:key", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var key = req.params("key");
    var id = toId(collection, key);
    var body = req.params("vertex");
    var g = Graph._graph(name);
    setResponse(res, "vertex", g[collection].update(id, body.forDB()));
  })
  .bodyParam("vertex", "The values that should be modified", Model)
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the vertex collection."
  })
  .pathParam("key", {
    type: "string",
    description: "_key attribute of one specific vertex."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The vertex does not exist.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

  /** Delete a vertex.
   *
   * Deletes a vertex with the given id, if it is contained
   * within the graph.
   * Furthermore all edges connected to this vertex will be deleted.
   */
  controller.del("/:graph/vertex/:collection/:key", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var key = req.params("key");
    var id = toId(collection, key);
    var g = Graph._graph(name);
    setResponse(res, "vertex", g[collection].remove(id));
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the vertex collection."
  })
  .pathParam("key", {
    type: "string",
    description: "_key attribute of one specific vertex."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The vertex does not exist.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

  /** Creates a new graph
   *
   * Creates a new graph object
   */
  controller.post("/", function(req, res) {
    var infos = req.params("graph");
    var g = Graph._create(infos.get("name"), infos.get("edgeDefinitions"));
    setResponse(res, "graph", {
      name: g.__name,
      edgeDefinitions: g.__edgeDefinitions
    }, actions.HTTP_CREATED);
  }).errorResponse(
    ArangoError, actions.HTTP_CONFLICT, "Graph creation error.", function(e) {
      return {
        code: actions.HTTP_CONFLICT,
        error: e.errorMessage
      };
    }
  ).bodyParam("graph", "The required information for a graph", Model);

  /** Drops an existing graph
   *
   * Drops an existing graph object by name.
   * By default all collections not used by other graphs will be dropped as
   * well. It can be optionally configured to not drop the collections.
   */
  controller.del("/:graph", function(req, res) {
    var name = req.params("graph");
    Graph._drop(name);
    setResponse(res);
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The graph does not exist.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );




}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
