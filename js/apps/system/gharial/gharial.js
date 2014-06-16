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
    _ = require("underscore"),
    errors = require("internal").errors,
    toId = function(c, k) {
      return c + "/" + k;
    },
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
    },
    setGraphResponse = function(res, g, code) {
      code = code || actions.HTTP_OK;
      setResponse(res, "graph", {
        name: g.__name,
        edgeDefinitions: g.__edgeDefinitions,
        orphanCollections: g._getOrphanCollections()
      }, code);
    };

////////////////////// Graph Creation /////////////////////////////////

  /** List graphs
   *
   * Creates a list of all available graphs.
   */
  controller.get("/", function(req, res) {
    setResponse(res, "graphs", Graph._list());
  });

  /** Creates a new graph
   *
   * Creates a new graph object
   */
  controller.post("/", function(req, res) {
    var infos = req.params("graph");
    var g = Graph._create(infos.get("name"), infos.get("edgeDefinitions"));
    setGraphResponse(res, g, actions.HTTP_CREATED);
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

/////////////////////// Definitions ////////////////////////////////////

  /** List all vertex collections.
   *
   * Gets the list of all vertex collections.
   */
  controller.get("/:graph/vertex", function(req, res) {
    var name = req.params("graph");
    var g = Graph._graph(name);
    setResponse(res, "collections", _.map(g._vertexCollections(), function(c) {
      return c.name();
    }).sort());
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The graph could not be found.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

  /** Create a new vertex collection.
   *
   * Stores a new vertex collection.
   * This has to contain the vertex-collection name.
   */
  controller.post("/:graph/vertex", function(req, res) {
    var name = req.params("graph");
    var body = req.params("collection");
    var g = Graph._graph(name);
    g._addOrphanCollection(body.get("collection"));
    setGraphResponse(res, g);
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .bodyParam(
    "collection", "The vertex collection to be stored.", Model
  )
  .errorResponse(
    ArangoError, actions.HTTP_BAD, "The vertex collection is invalid.", function(e) {
      return {
        code: actions.HTTP_BAD,
        error: e.errorMessage
      };
    }
  );

  /** Delete a vertex collection.
   *
   * Removes a vertex collection from this graph.
   * If this collection is used in one or more edge definitions 
   * All data stored in the collection is dropped as well as long
   * as it is not used in other graphs.
   */
  controller.del("/:graph/vertex/:collection", function(req, res) {
    var name = req.params("graph");
    var def_name = req.params("collection");
    var g = Graph._graph(name);
    g._removeOrphanCollection(def_name);
    setGraphResponse(res, g);
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the vertex collection."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND,
    "The collection is not found or part of an edge definition."
  );

  /** List all edge collections.
   *
   * Get the list of all edge collection.
   */
  controller.get("/:graph/edge", function(req, res) {
    var name = req.params("graph");
    var g = Graph._graph(name);
    setResponse(res, "collections", _.map(g._edgeCollections(), function(c) {
      return c.name();
    }).sort());
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The graph could not be found.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

  /** Create a new edge definition.
   *
   * Stores a new edge definition with the information contained
   * within the body.
   * This has to contain the edge-collection name, as well as set of from and to
   * collections-names respectively. 
   */
  controller.post("/:graph/edge", function(req, res) {
    var name = req.params("graph");
    var body = req.params("edgeDefinition");
    var g = Graph._graph(name);
    g._extendEdgeDefinitions(body.forDB());
    setGraphResponse(res, g);
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .bodyParam(
    "edgeDefinition", "The edge definition to be stored.", Model
  )
  .errorResponse(
    ArangoError, actions.HTTP_BAD, "The edge definition is invalid.", function(e) {
      return {
        code: actions.HTTP_BAD,
        error: e.errorMessage
      };
    }
  );

  /** Replace an edge definition.
   *
   * Replaces an existing edge definition with the information contained
   * within the body.
   * This has to contain the edge-collection name, as well as set of from and to
   * collections-names respectively.
   * This will also change the edge definitions of all other graphs using this
   * definition as well.
   */
  controller.put("/:graph/edge/:definition", function(req, res) {
    var name = req.params("graph");
    var def_name = req.params("definition");
    var body = req.params("edgeDefinition");
    var g = Graph._graph(name);
    if (def_name !== body.get("collection")) {
      var err = new ArangoError();
      err.errorNum = errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code;
      err.errorMessage = errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message;
      throw err;
    }
    g._editEdgeDefinitions(body.forDB());
    setGraphResponse(res, g);
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("definition", {
    type: "string",
    description: "Name of the edge collection in the definition."
  })
  .bodyParam(
    "edgeDefinition", "The edge definition to be stored.", Model
  )
  .errorResponse(
    ArangoError, actions.HTTP_BAD, "The edge definition is invalid.", function(e) {
      return {
        code: actions.HTTP_BAD,
        error: e.errorMessage
      };
    }
  );

  /** Delete an edge definition.
   *
   * Removes an existing edge definition from this graph.
   * All data stored in the collections is dropped as well as long
   * as it is not used in other graphs.
   */
  controller.del("/:graph/edge/:definition", function(req, res) {
    var name = req.params("graph");
    var def_name = req.params("definition");
    var g = Graph._graph(name);
    g._deleteEdgeDefinition(def_name);
    setGraphResponse(res, g);
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("definition", {
    type: "string",
    description: "Name of the edge collection in the definition."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge definition is invalid.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

////////////////////// Vertex Operations /////////////////////////////////

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

//////////////////////////// Edge Operations //////////////////////////

  /** Create a new edge.
   *
   * Stores a new edge with the information contained
   * within the body into the given collection.
   */
  controller.post("/:graph/edge/:collection", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var body = req.params("edge");
    var from = body.get("_from");
    var to = body.get("_to");
    var err;
    if (!from || !to) {
      err = new ArangoError();
      err.errorNum = errors.ERROR_GRAPH_INVALID_EDGE.code;
      err.errorMessage = errors.ERROR_GRAPH_INVALID_EDGE.message;
      throw err;
    }
    var g = Graph._graph(name);
    setResponse(res, "edge", g[collection].save(from, to, body.forDB()));
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the edge collection."
  })
  .bodyParam(
    "edge", "The edge to be stored. Has to contain _from and _to attributes.", Model
  )
  .errorResponse(
    ArangoError, actions.HTTP_BAD_REQUEST, "The edge could not be created.", function(e) {
      return {
        code: actions.HTTP_BAD_REQUEST,
        error: e.errorMessage
      };
    }
  );

  /** Load an edge.
   *
   * Loads an edge with the given id if it is contained
   * within your graph.
   */
  controller.get("/:graph/edge/:collection/:key", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var key = req.params("key");
    var id = toId(collection, key);
    var g = Graph._graph(name);
    setResponse(res, "edge", g[collection].document(id));
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the edge collection."
  })
  .pathParam("key", {
    type: "string",
    description: "_key attribute of one specific edge."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge does not exist.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

  /** Replace an edge.
   *
   * Replaces an edge with the given id by the content in the body.
   * This will only run successfully if the edge is contained
   * within the graph.
   */
  controller.put("/:graph/edge/:collection/:key", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var key = req.params("key");
    var id = toId(collection, key);
    var body = req.params("edge");
    var g = Graph._graph(name);
    setResponse(res, "edge", g[collection].replace(id, body.forDB()));
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the edge collection."
  })
  .pathParam("key", {
    type: "string",
    description: "_key attribute of one specific edge."
  })
  .bodyParam("edge", "The document to be stored. _from and _to attributes are ignored", Model)
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge does not exist.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

  /** Update an edge.
   *
   * Updates an edge with the given id by adding the content in the body.
   * This will only run successfully if the edge is contained
   * within the graph.
   */
  controller.patch("/:graph/edge/:collection/:key", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var key = req.params("key");
    var id = toId(collection, key);
    var body = req.params("edge");
    var g = Graph._graph(name);
    setResponse(res, "edge", g[collection].update(id, body.forDB()));
  })
  .bodyParam(
    "edge", "The values that should be modified. _from and _to attributes are ignored", Model
  )
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the edge collection."
  })
  .pathParam("key", {
    type: "string",
    description: "_key attribute of one specific edge."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge does not exist.", function(e) {
      return {
        code: actions.HTTP_NOT_FOUND,
        error: e.errorMessage
      };
    }
  );

  /** Delete an edge.
   *
   * Deletes an edge with the given id, if it is contained
   * within the graph.
   */
  controller.del("/:graph/edge/:collection/:key", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var key = req.params("key");
    var id = toId(collection, key);
    var g = Graph._graph(name);
    setResponse(res, "edge", g[collection].remove(id));
  })
  .pathParam("graph", {
    type: "string",
    description: "Name of the graph."
  })
  .pathParam("collection", {
    type: "string",
    description: "Name of the edge collection."
  })
  .pathParam("key", {
    type: "string",
    description: "_key attribute of one specific edge."
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge does not exist.", function(e) {
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
