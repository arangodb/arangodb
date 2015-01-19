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
    cluster = require("org/arangodb/cluster"),
    ArangoError = require("org/arangodb").ArangoError,
    actions = require("org/arangodb/actions"),
    Model = require("org/arangodb/foxx").Model,
    Graph = require("org/arangodb/general-graph"),
    _ = require("underscore"),
    joi = require("joi"),
    arangodb = require("org/arangodb"),
    errors = arangodb.errors,
    toId = function(c, k) {
      return c + "/" + k;
    },
    collectionRepresentation = function(collection, showProperties, showCount, showFigures) {
      var result = {};
      result.id = collection._id;
      result.name = collection.name();
      result.isSystem = (result.name.charAt(0) === '_');
      if (showProperties) {
        var properties = collection.properties();
        result.doCompact     = properties.doCompact;
        result.isVolatile    = properties.isVolatile;
        result.journalSize   = properties.journalSize;
        result.keyOptions    = properties.keyOptions;
        result.waitForSync   = properties.waitForSync;
        if (cluster.isCoordinator()) {
          result.shardKeys = properties.shardKeys;
          result.numberOfShards = properties.numberOfShards;
        }
      }

      if (showCount) {
        result.count = collection.count();
      }

      if (showFigures) {
        var figures = collection.figures();

        if (figures) {
          result.figures = figures;
        }
      }

      result.status = collection.status();
      result.type = collection.type();

      return result;
    },
    buildError = function(err, code) {
      return {
        error: true,
        code: code || actions.HTTP_BAD,
        errorNum: err.errorNum,
        errorMessage: err.errorMessage
      };
    },
    checkCollection = function(g, collection) {
      if (g[collection] === undefined) {
        var err;
        err = new ArangoError();
        err.errorNum = arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
        err.errorMessage = arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.message;
        throw err;
      }
    },
    setOptions = function(req) {
      var options = {
        code: actions.HTTP_ACCEPTED
      };
      options.waitForSync = Boolean(req.params("waitForSync"));
      if (options.waitForSync) {
        options.code = actions.HTTP_OK;
      }
      options.keepNull = Boolean(req.params("keepNull"));
      return options;
    },
    setResponse = function (res, name, body, code) {
      var obj = {};
      obj.error = false;
      obj.code = code || actions.HTTP_ACCEPTED; // Never wait for sync
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

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if a "if-match" or "if-none-match" error happens
////////////////////////////////////////////////////////////////////////////////
    matchError = function (req, res, doc, errorCode) {

      if (req.headers["if-none-match"] !== undefined) {
        var options = setOptions(req);
        if (options.code === actions.HTTP_OK) {
          options.code = actions.HTTP_CREATED;
        }
        if (doc._rev === req.headers["if-none-match"].replace(/(^["']|["']$)/g, '')) {
          // error
          res.responseCode = actions.HTTP_NOT_MODIFIED;
          res.contentType = "application/json; charset=utf-8";
          res.body = '';
          res.headers = {};
          return true;
        }
      }

      if (req.headers["if-match"] !== undefined) {
        if (doc._rev !== req.headers["if-match"].replace(/(^["']|["']$)/g, '')) {
          // error
          actions.resultError(req,
                              res,
                              actions.HTTP_PRECONDITION_FAILED,
                              errorCode,
                              "wrong revision",
                              {});
          return true;
        }
      }

      var rev = req.parameters.rev;
      if (rev !== undefined) {
        if (doc._rev !== rev) {
          // error
          actions.resultError(req,
                              res,
                              actions.HTTP_PRECONDITION_FAILED,
                              errorCode,
                              "wrong revision",
                              {});
          return true;
        }
      }

      return false;
    },

    setGraphResponse = function(res, g, code) {
      code = code || actions.HTTP_OK;
      setResponse(res, "graph", {
        name: g.__name,
        edgeDefinitions: g.__edgeDefinitions,
        orphanCollections: g._orphanCollections(),
        _id : g.__id,
        _rev : g.__rev
      }, code);
    },

    graphName = joi.string()
    .description("Name of the graph."),
    vertexCollectionName = joi.string()
    .description("Name of the vertex collection."),
    edgeCollectionName = joi.string()
    .description("Name of the edge collection."),
    dropCollectionFlag = joi.alternatives().try(joi.boolean(), joi.number().integer())
    .description("Flag to drop collection as well."),
    definitionEdgeCollectionName = joi.string()
    .description("Name of the edge collection in the definition."),
    waitForSyncFlag = joi.alternatives().try(joi.boolean(), joi.number().integer())
    .description("define if the request should wait until synced to disk."),
    vertexKey = joi.string()
    .description("_key attribute of one specific vertex"),
    edgeKey = joi.string()
    .description("_key attribute of one specific edge."),
    keepNullFlag = joi.alternatives().try(joi.boolean(), joi.number().integer())
    .description("define if null values should not be deleted.");

////////////////////// Graph Creation /////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_list_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialList}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   examples.loadGraph("routeplanner");
///   var url = "/_api/gharial";
///   var response = logCurlRequest('GET', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////


  /** List graphs
   *
   * Creates a list of all available graphs.
   */
  controller.get("/", function(req, res) {
    setResponse(res, "graphs", Graph._listObjects());
  });

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_create_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialCreate}
///   var graph = require("org/arangodb/general-graph");
/// | if (graph._exists("myGraph")) {
/// |    graph._drop("myGraph", true);
///   }
///   var url = "/_api/gharial";
///   body = {
///     name: "myGraph",
///     edgeDefinitions: [{
///       collection: "edges",
///       from: [ "startVertices" ],
///       to: [ "endVertices" ]
///     }]
///   };
///
///   var response = logCurlRequest('POST', url, body);
///
///   assert(response.code === 201);
///
///   logJsonResponse(response);
///
///   graph._drop("myGraph", true);
///
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  /** Creates a new graph
   *
   * Creates a new graph object
   */
  controller.post("/", function(req, res) {
    var infos = req.params("graph");
    var options = setOptions(req);
    if (options.code === actions.HTTP_OK) {
      options.code = actions.HTTP_CREATED;
    }
    var g = Graph._create(
      infos.get("name"),
      infos.get("edgeDefinitions"),
      infos.get("orphanCollections"),
      options
    );
    setGraphResponse(res, g, actions.HTTP_CREATED);
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_CONFLICT, "Graph creation error.", function(e) {
      return buildError(e, actions.HTTP_CONFLICT);
    }
  )
  .bodyParam("graph", {
    description: "The required information for a graph",
    type: Model
  });

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_get_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialGetGraph}
///   var graph = require("org/arangodb/general-graph");
/// | if (graph._exists("myGraph")) {
/// |    graph._drop("myGraph", true);
///   }
///   graph._create("myGraph", [{
///     collection: "edges",
///     from: [ "startVertices" ],
///     to: [ "endVertices" ]
///   }]);
///   var url = "/_api/gharial/myGraph";
///
///   var response = logCurlRequest('GET', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
///
///   graph._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  /** Get information of a graph
   *
   * Selects information for a given graph.
   * Will return the edge definitions as well as the vertex collections.
   * Or throws a 404 if the graph does not exist.
   */
  controller.get("/:graph", function(req, res) {
    var name = req.params("graph");
    var g = Graph._graph(name);
    setGraphResponse(res, g, actions.HTTP_OK);
  }).pathParam("graph", {
    type: graphName
  }).errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "Graph could not be found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_drop_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialDrop}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social";
///   var response = logCurlRequest('DELETE', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
///   db._drop("male");
///   db._drop("female");
///   db._drop("relation");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  /** Drops an existing graph
   *
   * Drops an existing graph object by name.
   * Optionally all collections not used by other graphs can be dropped as
   * well.
   */
  controller.del("/:graph", function(req, res) {
    var name = req.params("graph"),
        drop = Boolean(req.params("dropCollections"));
    Graph._drop(name, drop);
    setResponse(res, "removed", true, actions.HTTP_OK);
  })
  .pathParam("graph", {
    type: graphName
  })
  .queryParam("dropCollections", {
    type: dropCollectionFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The graph does not exist.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

/////////////////////// Definitions ////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_list_vertex_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialListVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/vertex";
///   var response = logCurlRequest('GET', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////


  /** List all vertex collections.
   *
   * Gets the list of all vertex collections.
   */
  controller.get("/:graph/vertex", function(req, res) {
    var name = req.params("graph");
    var g = Graph._graph(name);
    var mapFunc;
    if (req.params("collectionObjects")) {
      mapFunc = function(c) {
        return collectionRepresentation(c, false, false, false);
      };
    } else {
      mapFunc = function(c) {
        return c.name();
      };
    }
    setResponse(res, "collections", _.map(g._vertexCollections(), mapFunc).sort(), actions.HTTP_OK);
  })
  .pathParam("graph", {
    type: graphName
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The graph could not be found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_add_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialAddVertexCol}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/vertex";
///   body = {
///     collection: "otherVertices"
///   };
///   var response = logCurlRequest('POST', url, body);
///
///   assert(response.code === 201);
///
///   logJsonResponse(response);
///   examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  /** Create a new vertex collection.
   *
   * Stores a new vertex collection.
   * This has to contain the vertex-collection name.
   */
  controller.post("/:graph/vertex", function(req, res) {
    var name = req.params("graph");
    var body = req.params("collection");
    var g;
    try {
      g = Graph._graph(name);
    } catch (e) {
      var err = new Error();
      err.errorNum = e.errorNum;
      err.errorMessage = e.errorMessage;
      throw err;
    }
    g._addVertexCollection(body.get("collection"));
    setGraphResponse(res, g, actions.HTTP_CREATED);
  })
  .pathParam("graph", {
    type: graphName
  })
  .bodyParam("collection", {
    description: "The vertex collection to be stored.",
    type: Model
  })
  .errorResponse(
    Error, actions.HTTP_NOT_FOUND, "The graph could not be found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  )
  .errorResponse(
    ArangoError, actions.HTTP_BAD, "The vertex collection is invalid.", function(e) {
      return buildError(e);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_remove_http_examples
///
/// You can remove vertex collections that are not used in any edge collection:
///
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialRemoveVertexCollection}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   g._addVertexCollection("otherVertices");
///   var url = "/_api/gharial/social/vertex/otherVertices";
///   var response = logCurlRequest('DELETE', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// You cannot remove vertex collections that are used in edge collections:
///
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialRemoveVertexCollectionFailed}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var url = "/_api/gharial/social/vertex/male";
///   var response = logCurlRequest('DELETE', url);
///
///   assert(response.code === 400);
///
///   logJsonResponse(response);
///   db._drop("male");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  /** Delete a vertex collection.
   *
   * Removes a vertex collection from this graph.
   * If this collection is used in one or more edge definitions
   */
  controller.del("/:graph/vertex/:collection", function(req, res) {
    var name = req.params("graph");
    var def_name = req.params("collection");
    var g;
    var err;
    try {
      g = Graph._graph(name);
    } catch (e) {
      err = new Error();
      err.errorNum = e.errorNum;
      err.errorMessage = e.errorMessage;
      throw err;
    }
    if (g[def_name] === undefined) {
      err = new Error();
      err.errorNum = errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
      err.errorMessage = errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message;
      throw err;
    }
    var drop = Boolean(req.params("dropCollection"));
    g._removeVertexCollection(def_name, drop);
    setGraphResponse(res, g);
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: vertexCollectionName
  })
  .queryParam("dropCollection", {
    type: dropCollectionFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_BAD,
    "The collection is not found or part of an edge definition.", function(e) {
      return buildError(e, actions.HTTP_BAD);
    }
  )
  .errorResponse(
    Error, actions.HTTP_NOT_FOUND, "The graph could not be found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_list_edge_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialListEdge}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge";
///   var response = logCurlRequest('GET', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  /** List all edge collections.
   *
   * Get the list of all edge collection.
   */
  controller.get("/:graph/edge", function(req, res) {
    var name = req.params("graph");
    var g = Graph._graph(name);
    setResponse(res, "collections", _.map(g._edgeCollections(), function(c) {
      return c.name();
    }).sort(), actions.HTTP_OK);
  })
  .pathParam("graph", {
    type: graphName
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The graph could not be found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_definition_add_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialAddEdgeCol}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge";
///   body = {
///     collection: "lives_in",
///     from: ["female", "male"],
///     to: ["city"]
///   };
///   var response = logCurlRequest('POST', url, body);
///
///   assert(response.code === 201);
///
///   logJsonResponse(response);
///   examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    var g;
    try {
      g = Graph._graph(name);
    } catch (e) {
      var err = new Error();
      err.errorNum = e.errorNum;
      err.errorMessage = e.errorMessage;
      throw err;
    }
    g._extendEdgeDefinitions(body.forDB());
    setGraphResponse(res, g, actions.HTTP_CREATED);
  })
  .pathParam("graph", {
    type: graphName
  })
  .bodyParam("edgeDefinition", {
    description: "The edge definition to be stored.",
    type: Model
  })
  .errorResponse(
    Error, actions.HTTP_NOT_FOUND, "The graph could not be found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  )
  .errorResponse(
    ArangoError, actions.HTTP_BAD, "The edge definition is invalid.", function(e) {
      return buildError(e);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_definition_modify_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialReplaceEdgeCol}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge/relation";
///   body = {
///     collection: "relation",
///     from: ["female", "male", "animal"],
///     to: ["female", "male", "animal"]
///   };
///   var response = logCurlRequest('PUT', url, body);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
///   examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    var g;
    var err;
    try {
      g = Graph._graph(name);
    } catch (e) {
      err = new Error();
      err.errorNum = e.errorNum;
      err.errorMessage = e.errorMessage;
      throw err;
    }
    if (def_name !== body.get("collection")) {
      err = new ArangoError();
      err.errorNum = errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code;
      err.errorMessage = errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message;
      throw err;
    }
    g._editEdgeDefinitions(body.forDB());
    setGraphResponse(res, g);
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("definition", {
    type: definitionEdgeCollectionName
  })
  .bodyParam("edgeDefinition", {
    description: "The edge definition to be stored.",
    type: Model
  })
  .errorResponse(
    Error, actions.HTTP_NOT_FOUND, "The graph could not be found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  )
  .errorResponse(
    ArangoError, actions.HTTP_BAD, "The edge definition is invalid.", function(e) {
      return buildError(e);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_definition_remove_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialEdgeDefinitionRemove}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge/relation";
///   var response = logCurlRequest('DELETE', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
///   db._drop("relation");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
//
  /** Delete an edge definition.
   *
   * Removes an existing edge definition from this graph.
   * All data stored in the edge collection are dropped as well as long
   * as it is not used in other graphs.
   */
  controller.del("/:graph/edge/:definition", function(req, res) {
    var name = req.params("graph");
    var def_name = req.params("definition");
    var g;
    try {
      g = Graph._graph(name);
    } catch (e) {
      var err = new Error();
      err.errorNum = e.errorNum;
      err.errorMessage = e.errorMessage;
      throw err;
    }
    var drop = Boolean(req.params("dropCollection"));
    g._deleteEdgeDefinition(def_name, drop);
    setGraphResponse(res, g);
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("definition", {
    type: definitionEdgeCollectionName
  })
  .queryParam("dropCollection", {
    type: dropCollectionFlag
  })
  .errorResponse(
    Error, actions.HTTP_NOT_FOUND, "The graph could not be found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  )
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge definition is invalid.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////// Vertex Operations /////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_create_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialAddVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/vertex/male";
///   body = {
///     name: "Francis"
///   }
///   var response = logCurlRequest('POST', url, body);
///
///   assert(response.code === 202);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    var options = setOptions(req);
    if (options.code === actions.HTTP_OK) {
      options.code = actions.HTTP_CREATED;
    }
    checkCollection(g, collection);
    setResponse(res, "vertex", g[collection].save(body.forDB(), options), options.code);
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: vertexCollectionName
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "Graph or collection not found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  )
  .bodyParam("vertex", {
    description: "The document to be stored",
    type: Model
  });

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_get_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialGetVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/vertex/female/alice";
///   var response = logCurlRequest('GET', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
///   examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  /** Get a vertex.
   *
   * Gets a vertex with the given key if it is contained
   * within your graph.
   */
  controller.get("/:graph/vertex/:collection/:key", function(req, res) {
    var name = req.params("graph");
    var collection = req.params("collection");
    var key = req.params("key");
    var id = toId(collection, key);
    var g = Graph._graph(name);
    checkCollection(g, collection);
    var doc = g[collection].document(id);
    if (!matchError(req, res, doc, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
      setResponse(res, "vertex", doc, actions.HTTP_OK);
    }
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: vertexCollectionName
  })
  .pathParam("key", {
    type: vertexKey
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The vertex does not exist.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_replace_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialReplaceVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   body = {
///     name: "Alice Cooper",
///     age: 26
///   }
///   var url = "/_api/gharial/social/vertex/female/alice";
///   var response = logCurlRequest('PUT', url, body);
///
///   assert(response.code === 202);
///
///   logJsonResponse(response);
///   examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    checkCollection(g, collection);
    var doc = g[collection].document(id);
    var options = setOptions(req);
    if (!matchError(req, res, doc, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
      setResponse(res, "vertex", g[collection].replace(id, body.forDB(), options), options.code);
    }
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: vertexCollectionName
  })
  .pathParam("key", {
    type: vertexKey
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .bodyParam("vertex", {
    description: "The document to be stored",
    type: Model
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The vertex does not exist.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_modify_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialModifyVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   body = {
///     age: 26
///   }
///   var url = "/_api/gharial/social/vertex/female/alice";
///   var response = logCurlRequest('PATCH', url, body);
///
///   assert(response.code === 202);
///
///   logJsonResponse(response);
///   examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    checkCollection(g, collection);
    var doc = g[collection].document(id);
    var options = setOptions(req);
    if (!matchError(req, res, doc, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
      setResponse(res, "vertex", g[collection].update(id, body.forDB(), options), options.code);
    }
  })
  .bodyParam("vertex", {
    description: "The values that should be modified",
    type: Model
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: vertexCollectionName
  })
  .pathParam("key", {
    type: vertexKey
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .queryParam("keepNull", {
    type: keepNullFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The vertex does not exist.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_delete_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialDeleteVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/vertex/female/alice";
///   var response = logCurlRequest('DELETE', url);
///
///   assert(response.code === 202);
///
///   logJsonResponse(response);
///   examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    checkCollection(g, collection);
    var doc = g[collection].document(id);
    var options = setOptions(req);
    if (!matchError(req, res, doc, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
      var didRemove = g[collection].remove(id, options);
      setResponse(res, "removed", didRemove, options.code);
    }
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: vertexCollectionName
  })
  .pathParam("key", {
    type: vertexKey
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The vertex does not exist.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

//////////////////////////// Edge Operations //////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_create_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialAddEdge}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge/relation";
///   body = {
///     type: "friend",
///     _from: "female/alice",
///     _to: "female/diana"
///   };
///   var response = logCurlRequest('POST', url, body);
///
///   assert(response.code === 202);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    if (!from || !to) {
      throw new Error();
    }
    var g = Graph._graph(name);
    checkCollection(g, collection);
    var options = setOptions(req);
    if (options.code === actions.HTTP_OK) {
      options.code = actions.HTTP_CREATED;
    }
    try {
      setResponse(res, "edge", g[collection].save(from, to, body.forDB(), options), options.code);
    } catch(e) {
      if (e.errorNum === errors.ERROR_GRAPH_INVALID_EDGE.code) {
        throw new Error();
      }
      throw e;
    }
  })
  .bodyParam("edge", {
    description: "The edge to be stored. Has to contain _from and _to attributes.",
    type: Model
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: edgeCollectionName
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "Graph or collection not found.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  )
  .errorResponse(
    Error, actions.HTTP_BAD, "Edge is invalid.", function() {
      return buildError({
        errorNum: errors.ERROR_GRAPH_INVALID_EDGE.code,
        errorMessage: errors.ERROR_GRAPH_INVALID_EDGE.message
      }, actions.HTTP_BAD);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_get_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialGetEdge}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge/relation/aliceAndBob";
///   var response = logCurlRequest('GET', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    checkCollection(g, collection);
    var doc = g[collection].document(id);
    if (!matchError(req, res, doc, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
      setResponse(res, "edge", doc, actions.HTTP_OK);
    }
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: edgeCollectionName
  })
  .pathParam("key", {
    type: edgeKey
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge does not exist.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_replace_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialPutEdge}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge/relation/aliceAndBob";
///   body = {
///     type: "divorced"
///   }
///   var response = logCurlRequest('PUT', url, body);
///
///   assert(response.code === 202);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    checkCollection(g, collection);
    var doc = g[collection].document(id);
    var options = setOptions(req);
    if (!matchError(req, res, doc, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
      setResponse(res, "edge", g[collection].replace(id, body.forDB(), options), options.code);
    }
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: edgeCollectionName
  })
  .pathParam("key", {
    type: edgeKey
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .bodyParam("edge", {
    description: "The document to be stored. _from and _to attributes are ignored",
    type: Model
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge does not exist.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_modify_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialPatchEdge}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge/relation/aliceAndBob";
///   body = {
///     since: "01.01.2001"
///   }
///   var response = logCurlRequest('PATCH', url, body);
///
///   assert(response.code === 202);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
    checkCollection(g, collection);
    var doc = g[collection].document(id);
    var options = setOptions(req);
    if (!matchError(req, res, doc, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
      setResponse(res, "edge", g[collection].update(id, body.forDB(), options), options.code);
    }
  })
  .bodyParam("edge", {
    description: "The values that should be modified. _from and _to attributes are ignored",
    type: Model
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: edgeCollectionName
  })
  .pathParam("key", {
    type: edgeKey
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .queryParam("keepNull", {
    type: keepNullFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge does not exist.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
    }
  );

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_delete_http_examples
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialDeleteEdge}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge/relation/aliceAndBob";
///   var response = logCurlRequest('DELETE', url);
///
///   assert(response.code === 202);
///
///   logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////


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
    checkCollection(g, collection);
    var doc = g[collection].document(id);
    var options = setOptions(req);
    if (!matchError(req, res, doc, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
      var didRemove = g[collection].remove(id, options);
      setResponse(res, "removed", didRemove, options.code);
    }
  })
  .pathParam("graph", {
    type: graphName
  })
  .pathParam("collection", {
    type: edgeCollectionName
  })
  .pathParam("key", {
    type: edgeKey
  })
  .queryParam("waitForSync", {
    type: waitForSyncFlag
  })
  .errorResponse(
    ArangoError, actions.HTTP_NOT_FOUND, "The edge does not exist.", function(e) {
      return buildError(e, actions.HTTP_NOT_FOUND);
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
