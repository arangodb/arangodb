/*global applicationContext*/

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
/// @brief Lists all graphs known to the graph module.
/// 
/// @RESTHEADER{GET /_api/gharial, List all graphs}
/// 
/// @RESTDESCRIPTION
/// Lists all graph names stored in this database.ssss
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Is returned if the module is available and the graphs could be listed.
/// 
/// @EXAMPLES
/// 
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
/// ~ examples.dropGraph("social");
/// ~ examples.dropGraph("routeplanner");
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
/// @brief Create a new graph in the graph module.
/// 
/// @RESTHEADER{POST /_api/gharial, Create a graph}
/// 
/// @RESTDESCRIPTION
/// The creation of a graph requires the name of the graph and a definition of its edges.
/// 
/// @RESTBODYPARAM{name,string,required,string}
/// Name of the graph.
/// 
/// @RESTBODYPARAM{edgeDefinitions,string,optional,string}
/// An array of definitions for the edges, see [edge definitions](../General-Graphs/Management.md#edge_definitions).
/// 
/// @RESTBODYPARAM{orphanCollections,string,optional,string}
/// An array of additional vertex collections.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// Is returned if the graph could be created.
/// The body contains the graph configuration that has been stored.
/// 
/// @RESTRETURNCODE{409}
/// Returned if there is a conflict storing the graph.
/// This can occur either if a graph with this name is already stored, or if there is one edge definition with a
/// the same [edge collection](../Glossary/index.html#edge_collection)
/// but a different signature used in any other graph.
/// 
/// @EXAMPLES
/// 
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
/// @brief Get a graph from the graph module.
/// 
/// @RESTHEADER{GET /_api/gharial/{graph-name}, Get a graph}
/// 
/// @RESTDESCRIPTION
/// Gets a graph from the collection *\_graphs*.
/// Returns the definition content of this graph.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the graph could be found.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
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
/// @brief delete an existing graph
/// 
/// @RESTHEADER{DELETE /_api/gharial/{graph-name}, Drop a graph}
/// 
/// @RESTDESCRIPTION
/// Removes a graph from the collection *\_graphs*.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{dropCollections, boolean, optional}
/// Drop collections of this graph as well.
/// Collections will only be dropped if they are not used in other graphs.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the graph could be dropped.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
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
/// ~ examples.dropGraph("social");
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
/// @brief Lists all vertex collections used in this graph.
/// 
/// @RESTHEADER{GET /_api/gharial/{graph-name}/vertex, List vertex collections}
/// 
/// @RESTDESCRIPTION
/// Lists all vertex collections within this graph.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Is returned if the collections could be listed.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialListVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/vertex";
///   var response = logCurlRequest('GET', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
/// ~ examples.dropGraph("social");
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
/// @brief Add an additional vertex collection to the graph.
/// 
/// @RESTHEADER{POST /_api/gharial/{graph-name}/vertex, Add vertex collection}
/// 
/// @RESTDESCRIPTION
/// Adds a vertex collection to the set of collections of the graph. If the
/// collection does not exist, it will be created.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// Returned if the edge collection could be added successfully.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
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
/// @brief Remove a vertex collection form the graph.
/// 
/// @RESTHEADER{DELETE /_api/gharial/{graph-name}/vertex/{collection-name}, Remove vertex collection}
/// 
/// @RESTDESCRIPTION
/// Removes a vertex collection from the graph and optionally deletes the collection,
/// if it is not used in any other graph.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required}
/// The name of the vertex collection.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{dropCollection, boolean, optional}
/// Drop the collection as well.
/// Collection will only be dropped if it is not used in other graphs.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the vertex collection was removed from the graph successfully.
/// 
/// @RESTRETURNCODE{400}
/// Returned if the vertex collection is still used in an edge definition.
/// In this case it cannot be removed from the graph yet, it has to be removed from the edge definition first.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
/// ///
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
/// ~ examples.dropGraph("social");
/// ~ db._drop("otherVertices");
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
///   db._drop("female");
///   db._drop("relation");
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
/// @brief Lists all edge definitions
/// 
/// @RESTHEADER{GET /_api/gharial/{graph-name}/edge, List edge definitions}
/// 
/// @RESTDESCRIPTION
/// Lists all edge collections within this graph.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Is returned if the edge definitions could be listed.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialListEdge}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge";
///   var response = logCurlRequest('GET', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
/// ~ examples.dropGraph("social");
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
/// @brief Add a new edge definition to the graph
/// 
/// @RESTHEADER{POST /_api/gharial/{graph-name}/edge, Add edge definition}
/// 
/// @RESTDESCRIPTION
/// Adds an additional edge definition to the graph.
/// This edge definition has to contain a *collection* and an array of each *from* and *to* vertex collections.
/// An edge definition can only be added if this definition is either not used in any other graph, or it is used
/// with exactly the same definition. It is not possible to store a definition "e" from "v1" to "v2" in the one
/// graph, and "e" from "v2" to "v1" in the other graph.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTBODYPARAM{collection,string,required,string}
/// The name of the edge collection to be used.
/// 
/// @RESTBODYPARAM{from,array,required,string}
/// One or many vertex collections that can contain source vertices.
/// 
/// @RESTBODYPARAM{to,array,required,string}
/// One or many edge collections that can contain target vertices.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the definition could be added successfully.
/// 
/// @RESTRETURNCODE{400}
/// Returned if the defininition could not be added, the edge collection is used in an other graph with
/// a different signature.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
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
/// @brief Replace an existing edge definition
/// 
/// @RESTHEADER{POST /_api/gharial/{graph-name}/edge/{definition-name}, Replace an edge definition}
/// 
/// @RESTDESCRIPTION
/// Change one specific edge definition.
/// This will modify all occurrences of this definition in all graphs known to your database.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{definition-name, string, required}
/// The name of the edge collection used in the definition.
/// 
/// @RESTBODYPARAM{collection,string,required,string}
/// The name of the edge collection to be used.
/// 
/// @RESTBODYPARAM{from,array,required,string}
/// One or many vertex collections that can contain source vertices.
/// 
/// @RESTBODYPARAM{to,array,required,string}
/// One or many edge collections that can contain target vertices.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the edge definition could be replaced.
/// 
/// @RESTRETURNCODE{400}
/// Returned if no edge definition with this name is found in the graph.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
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
/// @brief Remove an edge definition form the graph
/// 
/// @RESTHEADER{DELETE /_api/gharial/{graph-name}/edge/{definition-name}, Remove an edge definition from the graph}
/// 
/// @RESTDESCRIPTION
/// Remove one edge definition from the graph.
/// This will only remove the edge collection, the vertex collections remain untouched and can still be used
/// in your queries.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{definition-name, string, required}
/// The name of the edge collection used in the definition.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{dropCollection, boolean, optional}
/// Drop the collection as well.
/// Collection will only be dropped if it is not used in other graphs.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the edge definition could be removed from the graph.
/// 
/// @RESTRETURNCODE{400}
/// Returned if no edge definition with this name is found in the graph.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name could be found.
/// 
/// @EXAMPLES
/// 
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
///   examples.dropGraph("social");
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
/// @brief create a new vertex
/// 
/// @RESTHEADER{POST /_api/gharial/{graph-name}/vertex/{collection-name}, Create a vertex}
/// 
/// @RESTDESCRIPTION
/// Adds a vertex to the given collection.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required} 
/// The name of the vertex collection the vertex belongs to.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{waitForSync, boolean, optional}
/// Define if the request should wait until synced to disk.
/// 
/// @RESTALLBODYPARAM{storeThisObject,object,required}
/// The body has to be the JSON object to be stored.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// Returned if the vertex could be added and waitForSync is true.
/// 
/// @RESTRETURNCODE{202}
/// Returned if the request was successful but waitForSync is false.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph or no vertex collection with this name could be found.
/// 
/// @EXAMPLES
/// 
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
///   examples.dropGraph("social");
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
/// @brief fetches an existing vertex
/// 
/// @RESTHEADER{GET /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}, Get a vertex}
/// 
/// @RESTDESCRIPTION
/// Gets a vertex from the given collection.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required} 
/// The name of the vertex collection the vertex belongs to.
/// 
/// @RESTPARAM{vertex-key, string, required} 
/// The *_key* attribute of the vertex.
/// 
/// @RESTHEADERPARAMETERS
/// 
/// @RESTPARAM{if-match, string, optional}
/// If the "If-Match" header is given, then it must contain exactly one etag. The document is returned,
/// if it has the same revision as the given etag. Otherwise a HTTP 412 is returned. As an alternative
/// you can supply the etag in an attribute rev in the URL.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the vertex could be found.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name, no vertex collection or no vertex with this id could be found.
/// 
/// @RESTRETURNCODE{412}
/// Returned if if-match header is given, but the documents revision is different.
/// 
/// @EXAMPLES
/// 
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
/// @brief replaces an existing vertex
/// 
/// @RESTHEADER{PUT /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}, Replace a vertex}
/// 
/// @RESTDESCRIPTION
/// Replaces the data of a vertex in the collection.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required} 
/// The name of the vertex collection the vertex belongs to.
/// 
/// @RESTPARAM{vertex-key, string, required} 
/// The *_key* attribute of the vertex.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{waitForSync, boolean, optional}
/// Define if the request should wait until synced to disk.
/// 
/// @RESTHEADERPARAMETERS
/// 
/// @RESTPARAM{if-match, string, optional}
/// If the "If-Match" header is given, then it must contain exactly one etag. The document is updated,
/// if it has the same revision as the given etag. Otherwise a HTTP 412 is returned. As an alternative
/// you can supply the etag in an attribute rev in the URL.
/// 
/// @RESTALLBODYPARAM{storeThisJsonObject,object,required}
/// The body has to be the JSON object to be stored.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the vertex could be replaced.
/// 
/// @RESTRETURNCODE{202}
/// Returned if the request was successful but waitForSync is false.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name, no vertex collection or no vertex with this id could be found.
/// 
/// @RESTRETURNCODE{412}
/// Returned if if-match header is given, but the documents revision is different.
/// 
/// @EXAMPLES
/// 
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
/// @brief replace an existing vertex
/// 
/// @RESTHEADER{PATCH /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}, Modify a vertex}
/// 
/// @RESTDESCRIPTION
/// Updates the data of the specific vertex in the collection.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required} 
/// The name of the vertex collection the vertex belongs to.
/// 
/// @RESTPARAM{vertex-key, string, required} 
/// The *_key* attribute of the vertex.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{waitForSync, boolean, optional}
/// Define if the request should wait until synced to disk.
/// 
/// @RESTPARAM{keepNull, boolean, optional}
/// Define if values set to null should be stored. By default the key is removed from the document.
/// 
/// @RESTHEADERPARAMETERS
/// 
/// @RESTPARAM{if-match, string, optional}
/// If the "If-Match" header is given, then it must contain exactly one etag. The document is updated,
/// if it has the same revision as the given etag. Otherwise a HTTP 412 is returned. As an alternative
/// you can supply the etag in an attribute rev in the URL.
/// 
/// @RESTALLBODYPARAM{replaceAttributes,object,required}
/// The body has to contain a JSON object containing exactly the attributes that should be replaced.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the vertex could be updated.
/// 
/// @RESTRETURNCODE{202}
/// Returned if the request was successful but waitForSync is false.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name, no vertex collection or no vertex with this id could be found.
/// 
/// @RESTRETURNCODE{412}
/// Returned if if-match header is given, but the documents revision is different.
/// 
/// @EXAMPLES
/// 
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
/// @brief removes a vertex from a graph
/// 
/// @RESTHEADER{DELETE /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}, Remove a vertex}
/// 
/// @RESTDESCRIPTION
/// Removes a vertex from the collection.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required} 
/// The name of the vertex collection the vertex belongs to.
/// 
/// @RESTPARAM{vertex-key, string, required} 
/// The *_key* attribute of the vertex.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{waitForSync, boolean, optional}
/// Define if the request should wait until synced to disk.
/// 
/// @RESTHEADERPARAMETERS
/// 
/// @RESTPARAM{if-match, string, optional}
/// If the "If-Match" header is given, then it must contain exactly one etag. The document is updated,
/// if it has the same revision as the given etag. Otherwise a HTTP 412 is returned. As an alternative
/// you can supply the etag in an attribute rev in the URL.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the vertex could be removed.
/// 
/// @RESTRETURNCODE{202}
/// Returned if the request was successful but waitForSync is false.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name, no vertex collection or no vertex with this id could be found.
/// 
/// @RESTRETURNCODE{412}
/// Returned if if-match header is given, but the documents revision is different.
/// 
/// @EXAMPLES
/// 
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
/// @brief Creates an edge in an existing graph
///
/// @RESTHEADER{POST /_api/gharial/{graph-name}/edge/{collection-name}, Create an edge}
/// 
/// @RESTDESCRIPTION
/// Creates a new edge in the collection.
/// Within the body the has to contain a *\_from* and *\_to* value referencing to valid vertices in the graph.
/// Furthermore the edge has to be valid in the definition of this
/// [edge collection](../Glossary/index.html#edge_collection).
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required} 
/// The name of the edge collection the edge belongs to.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{waitForSync, boolean, optional}
/// Define if the request should wait until synced to disk.
/// 
/// @RESTPARAM{_from, string, required}
/// 
/// @RESTPARAM{_to, string, required}
/// 
/// @RESTALLBODYPARAM{storeThisJsonObject,object,required}
/// The body has to be the JSON object to be stored.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// Returned if the edge could be created.
/// 
/// @RESTRETURNCODE{202}
/// Returned if the request was successful but waitForSync is false.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name, no edge collection or no edge with this id could be found.
/// 
/// @EXAMPLES
/// 
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
///   examples.dropGraph("social");
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
/// @brief fetch an edge
/// 
/// @RESTHEADER{GET /_api/gharial/{graph-name}/edge/{collection-name}/{edge-key}, Get an edge}
/// 
/// @RESTDESCRIPTION
/// Gets an edge from the given collection.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required} 
/// The name of the edge collection the edge belongs to.
/// 
/// @RESTPARAM{edge-key, string, required} 
/// The *_key* attribute of the vertex.
/// 
/// @RESTHEADERPARAMETERS
/// 
/// @RESTPARAM{if-match, string, optional}
/// If the "If-Match" header is given, then it must contain exactly one etag. The document is returned,
/// if it has the same revision as the given etag. Otherwise a HTTP 412 is returned. As an alternative
/// you can supply the etag in an attribute rev in the URL.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the edge could be found.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name, no edge collection or no edge with this id could be found.
/// 
/// @RESTRETURNCODE{412}
/// Returned if if-match header is given, but the documents revision is different.
/// 
/// @EXAMPLES
/// 
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialGetEdge}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge/relation/aliceAndBob";
///   var response = logCurlRequest('GET', url);
///
///   assert(response.code === 200);
///
///   logJsonResponse(response);
///   examples.dropGraph("social");
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
/// @brief replace the content of an existing edge
///
/// @RESTHEADER{PUT /_api/gharial/{graph-name}/edge/{collection-name}/{edge-key}, Replace an edge}
/// 
/// @RESTDESCRIPTION
/// Replaces the data of an edge in the collection.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required} 
/// The name of the edge collection the edge belongs to.
/// 
/// @RESTPARAM{edge-key, string, required} 
/// The *_key* attribute of the vertex.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{waitForSync, boolean, optional}
/// Define if the request should wait until synced to disk.
/// 
/// @RESTHEADERPARAMETERS
/// 
/// @RESTPARAM{if-match, string, optional}
/// If the "If-Match" header is given, then it must contain exactly one etag. The document is updated,
/// if it has the same revision as the given etag. Otherwise a HTTP 412 is returned. As an alternative
/// you can supply the etag in an attribute rev in the URL.
/// 
/// @RESTALLBODYPARAM{storeThisJsonObject,object,required}
/// The body has to be the JSON object to be stored.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the edge could be replaced.
/// 
/// @RESTRETURNCODE{202}
/// Returned if the request was successful but waitForSync is false.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name, no edge collection or no edge with this id could be found.
/// 
/// @RESTRETURNCODE{412}
/// Returned if if-match header is given, but the documents revision is different.
/// 
/// @EXAMPLES
/// 
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
///   examples.dropGraph("social");
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
/// @brief modify an existing edge
///
/// @RESTHEADER{PATCH /_api/gharial/{graph-name}/edge/{collection-name}/{edge-key}, Modify an edge}
/// 
/// @RESTDESCRIPTION
/// Updates the data of the specific edge in the collection.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required}
/// The name of the edge collection the edge belongs to.
/// 
/// @RESTPARAM{edge-key, string, required}
/// The *_key* attribute of the vertex.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{waitForSync, boolean, optional}
/// Define if the request should wait until synced to disk.
/// 
/// @RESTPARAM{keepNull, boolean, optional}
/// Define if values set to null should be stored. By default the key is removed from the document.
/// 
/// @RESTALLBODYPARAM{updateAttributes,object,required}
/// The body has to be a JSON object containing the attributes to be updated.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the edge could be updated.
/// 
/// @RESTRETURNCODE{202}
/// Returned if the request was successful but waitForSync is false.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name, no edge collection or no edge with this id could be found.
/// 
/// @EXAMPLES
/// 
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
///   examples.dropGraph("social");
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
/// @brief removes an edge from graph
/// 
/// @RESTHEADER{DELETE /_api/gharial/{graph-name}/edge/{collection-name}/{edge-key}, Remove an edge}
/// 
/// @RESTDESCRIPTION
/// Removes an edge from the collection.
/// 
/// @RESTURLPARAMETERS
/// 
/// @RESTPARAM{graph-name, string, required}
/// The name of the graph.
/// 
/// @RESTPARAM{collection-name, string, required} 
/// The name of the edge collection the edge belongs to.
/// 
/// @RESTPARAM{edge-key, string, required} 
/// The *_key* attribute of the vertex.
/// 
/// @RESTQUERYPARAMETERS
/// 
/// @RESTPARAM{waitForSync, boolean, optional}
/// Define if the request should wait until synced to disk.
/// 
/// @RESTHEADERPARAMETERS
/// 
/// @RESTPARAM{if-match, string, optional}
/// If the "If-Match" header is given, then it must contain exactly one etag. The document is updated,
/// if it has the same revision as the given etag. Otherwise a HTTP 412 is returned. As an alternative
/// you can supply the etag in an attribute rev in the URL.
/// 
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// Returned if the edge could be removed.
/// 
/// @RESTRETURNCODE{202}
/// Returned if the request was successful but waitForSync is false.
/// 
/// @RESTRETURNCODE{404}
/// Returned if no graph with this name, no edge collection or no edge with this id could be found.
/// 
/// @RESTRETURNCODE{412}
/// Returned if if-match header is given, but the documents revision is different.
/// 
/// @EXAMPLES
/// 
/// @EXAMPLE_ARANGOSH_RUN{HttpGharialDeleteEdge}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   examples.loadGraph("social");
///   var url = "/_api/gharial/social/edge/relation/aliceAndBob";
///   var response = logCurlRequest('DELETE', url);
///
///   assert(response.code === 202);
///
///   logJsonResponse(response);
///   examples.dropGraph("social");
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
