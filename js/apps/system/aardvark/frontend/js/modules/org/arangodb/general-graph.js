module.define("org/arangodb/general-graph", function(exports, module) {
/*jshint strict: false */
/*global require, exports, ArangoClusterComm */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Florian Bartels, Michael Hackstein, Guido Schwab
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


var arangodb = require("org/arangodb"),
  internal = require("internal"),
  ArangoCollection = arangodb.ArangoCollection,
  ArangoError = arangodb.ArangoError,
  db = arangodb.db,
  errors = arangodb.errors,
  _ = require("underscore"),
  aqlGen = require("org/arangodb/graph/aql-generator"),
  transformExample = aqlGen.transformExample,
  stringToArray = aqlGen.stringToArray,
  AQLGenerator = aqlGen.AQLGenerator,

// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/general-graph"
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// --SECTION--                                                   query constants
// -----------------------------------------------------------------------------


  EDGES = "RETURN GRAPH_EDGES(@graphName,@vertexExample,@options)",
  COMMON_NEIGHBORS = "FOR e IN GRAPH_COMMON_NEIGHBORS(@graphName,@ex1,@ex2,"
    + "@options1,@options2)SORT ATTRIBUTES(e)[0] RETURN e",
  COUNT_COMMON_NEIGHBORS = "FOR e IN GRAPH_COMMON_NEIGHBORS(@graphName,@ex1,@ex2,"
    + "@options1,@options2) FOR a in ATTRIBUTES(e) FOR b in ATTRIBUTES(e[a]) "
    + "SORT ATTRIBUTES(e)[0] RETURN [a, b, LENGTH(e[a][b])]",
  COMMON_PROPERTIES = "FOR e IN GRAPH_COMMON_PROPERTIES(@graphName,@ex1,@ex2"
    + ",@options) SORT ATTRIBUTES(e)[0] RETURN e",
  COUNT_COMMON_PROPERTIES = "FOR e IN GRAPH_COMMON_PROPERTIES(@graphName,@ex1,@ex2"
    + ",@options) FOR a in ATTRIBUTES(e) SORT ATTRIBUTES(e)[0] RETURN [ATTRIBUTES(e)[0], LENGTH(e[a])]",
  PATHS = "RETURN GRAPH_PATHS(@graphName,@options)",
  SHORTEST_PATH = "RETURN GRAPH_SHORTEST_PATH(@graphName,@ex1,@ex2,@options)",
  DISTANCE_TO = "RETURN GRAPH_DISTANCE_TO(@graphName,@ex1,@ex2,@options)",
  ABSOLUTE_ECCENTRICITY = "RETURN GRAPH_ABSOLUTE_ECCENTRICITY(@graphName,@ex1,@options)",
  ECCENTRICITY = "RETURN GRAPH_ECCENTRICITY(@graphName,@options)"; 

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a parameter is not defined, an empty string or an empty
//  array
////////////////////////////////////////////////////////////////////////////////


var isValidCollectionsParameter = function (x) {
  if (!x) {
    return false;
  }
  if (Array.isArray(x) && x.length === 0) {
    return false;
  }
  if (typeof x !== "string" && !Array.isArray(x)) {
    return false;
  }
  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create a collection by name
////////////////////////////////////////////////////////////////////////////////

var findOrCreateCollectionByName = function (name, type, noCreate) {
  var col = db._collection(name),
    res = false;
  if (col === null && !noCreate) {
    if (type === ArangoCollection.TYPE_DOCUMENT) {
      col = db._create(name);
    } else {
      col = db._createEdgeCollection(name);
    }
    res = true;
  } else if (!(col instanceof ArangoCollection)) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NO_AN_ARANGO_COLLECTION.code;
    err.errorMessage = name + arangodb.errors.ERROR_GRAPH_NO_AN_ARANGO_COLLECTION.message;
    throw err;
  }
  return res;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief find or create a collection by name
////////////////////////////////////////////////////////////////////////////////

var findOrCreateCollectionsByEdgeDefinitions = function (edgeDefinitions, noCreate) {
  var vertexCollections = {},
  edgeCollections = {};
  edgeDefinitions.forEach(function (e) {
    if (! e.hasOwnProperty('collection') || 
        ! e.hasOwnProperty('from') ||
        ! e.hasOwnProperty('to') ||
        ! Array.isArray(e.from) ||
        ! Array.isArray(e.to)) {
      var err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.code;
      err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.message;
      throw err;
    }

    e.from.concat(e.to).forEach(function (v) {
      findOrCreateCollectionByName(v, ArangoCollection.TYPE_DOCUMENT, noCreate);
      vertexCollections[v] = db[v];
    });
    findOrCreateCollectionByName(e.collection, ArangoCollection.TYPE_EDGE, noCreate);
    edgeCollections[e.collection] = db[e.collection];
  });
  return [
    vertexCollections,
    edgeCollections
  ];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to get graphs collection
////////////////////////////////////////////////////////////////////////////////

var getGraphCollection = function() {
  var gCol = db._graphs;
  if (gCol === null || gCol === undefined) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NO_GRAPH_COLLECTION.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NO_GRAPH_COLLECTION.message;
    throw err;
  }
  return gCol;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to print edge definitions in _PRINT
////////////////////////////////////////////////////////////////////////////////

var printEdgeDefinitions = function(defs) {
  return _.map(defs, function(d) {
    var out = d.collection;
    out += ": [";
    out += d.from.join(", ");
    out += "] -> [";
    out += d.to.join(", ");
    out += "]";
    return out;
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to wrap arango collections for overwrite
////////////////////////////////////////////////////////////////////////////////

var wrapCollection = function(col) {
  var wrapper = {};
  _.each(_.functions(col), function(func) {
    wrapper[func] = function() {
      return col[func].apply(col, arguments);
    };
  });
  return wrapper;
};


// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/general-graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// Deprecated block
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_undirectedRelation
/// @brief Define an undirected relation.
///
/// `graph_module._undirectedRelation(relationName, vertexCollections)`
///
/// Defines an undirected relation with the name *relationName* using the
/// list of *vertexCollections*. This relation allows the user to store
/// edges in any direction between any pair of vertices within the
/// *vertexCollections*.
///
/// @PARAMS
///
/// @PARAM{relationName, string, required}
///   The name of the edge collection where the edges should be stored.
///   Will be created if it does not yet exist.
///
/// @PARAM{vertexCollections, array, required}
///   One or a list of collection names for which connections are allowed.
///   Will be created if they do not exist.
///
/// @EXAMPLES
///
/// To define simple relation with only one vertex collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphUndirectedRelationDefinition1}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._undirectedRelation("friend", "user");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To define a relation between several vertex collections:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphUndirectedRelationDefinition2}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._undirectedRelation("marriage", ["female", "male"]);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// Deprecated block
////////////////////////////////////////////////////////////////////////////////
var _undirectedRelation = function (relationName, vertexCollections) {
  var err;
  if (arguments.length < 2) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.message + "2";
    throw err;
  }

  if (typeof relationName !== "string" || relationName === "") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message + " arg1 must not be empty";
    throw err;
  }

  if (!isValidCollectionsParameter(vertexCollections)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message + " arg2 must not be empty";
    throw err;
  }

  return {
    collection: relationName,
    from: stringToArray(vertexCollections),
    to: stringToArray(vertexCollections)
  };
};
////////////////////////////////////////////////////////////////////////////////
/// Deprecated block
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_directedRelation
/// @brief Define a directed relation.
///
/// `graph_module._directedRelation(relationName, fromVertexCollections, toVertexCollections)`
///
/// The *relationName* defines the name of this relation and references to the underlying edge collection.
/// The *fromVertexCollections* is an Array of document collections holding the start vertices.
/// The *toVertexCollections* is an Array of document collections holding the target vertices.
/// Relations are only allowed in the direction from any collection in *fromVertexCollections*
/// to any collection in *toVertexCollections*.
///
/// @PARAMS
///
/// @PARAM{relationName, string, required}
///   The name of the edge collection where the edges should be stored.
///   Will be created if it does not yet exist.
///
/// @PARAM{fromVertexCollections, array, required}
///   One or a list of collection names. Source vertices for the edges
///   have to be stored in these collections. Collections will be created if they do not exist.
///
/// @PARAM{toVertexCollections, array, required}
///   One or a list of collection names. Target vertices for the edges
///   have to be stored in these collections. Collections will be created if they do not exist.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDirectedRelationDefinition}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._directedRelation("has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_relation
/// @brief Define a directed relation.
///
/// `graph_module._relation(relationName, fromVertexCollections, toVertexCollections)`
///
/// The *relationName* defines the name of this relation and references to the underlying edge collection.
/// The *fromVertexCollections* is an Array of document collections holding the start vertices.
/// The *toVertexCollections* is an Array of document collections holding the target vertices.
/// Relations are only allowed in the direction from any collection in *fromVertexCollections*
/// to any collection in *toVertexCollections*.
///
/// @PARAMS
///
/// @PARAM{relationName, string, required}
///   The name of the edge collection where the edges should be stored.
///   Will be created if it does not yet exist.
///
/// @PARAM{fromVertexCollections, array, required}
///   One or a list of collection names. Source vertices for the edges
///   have to be stored in these collections. Collections will be created if they do not exist.
///
/// @PARAM{toVertexCollections, array, required}
///   One or a list of collection names. Target vertices for the edges
///   have to be stored in these collections. Collections will be created if they do not exist.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRelationDefinition}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._relation("has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRelationDefinitionSingle}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._relation("has_bought", "Customer", "Product");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

var _relation = function (
  relationName, fromVertexCollections, toVertexCollections) {
  var err;
  if (arguments.length < 3) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.message + "3";
    throw err;
  }

  if (typeof relationName !== "string" || relationName === "") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message + " arg1 must be non empty string";
    throw err;
  }

  if (!isValidCollectionsParameter(fromVertexCollections)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message
      + " arg2 must be non empty string or array";
    throw err;
  }

  if (!isValidCollectionsParameter(toVertexCollections)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message
      + " arg3 must be non empty string or array";
    throw err;
  }

  return {
    collection: relationName,
    from: stringToArray(fromVertexCollections),
    to: stringToArray(toVertexCollections)
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_list
/// @brief List all graphs.
///
/// `graph_module._list()`
///
/// Lists all graph names stored in this database.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphList}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._list();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

var _list = function() {
  var gdb = getGraphCollection();
  return _.pluck(gdb.toArray(), "_key");
};


var _listObjects = function() {
  return getGraphCollection().toArray();
};




////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_definitions
/// @brief Create a list of edge definitions to construct a graph.
///
/// `graph_module._edgeDefinitions(relation1, relation2, ..., relationN)`
///
/// The list of edge definitions of a graph can be managed by the graph module itself.
/// This function is the entry point for the management and will return the correct list.
///
/// @PARAMS
///
/// @PARAM{relationX, object, optional}
/// An object representing a definition of one relation in the graph
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeDefinitions}
///   var graph_module = require("org/arangodb/general-graph");
///   directed_relation = graph_module._relation("lives_in", "user", "city");
///   undirected_relation = graph_module._relation("knows", "user", "user");
///   edgedefinitions = graph_module._edgeDefinitions(directed_relation, undirected_relation);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////


var _edgeDefinitions = function () {

  var res = [], args = arguments;
  Object.keys(args).forEach(function (x) {
   res.push(args[x]);
  });

  return res;

};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_extend_edge_definitions
/// @brief Extend the list of edge definitions to construct a graph.
///
/// `graph_module._extendEdgeDefinitions(edgeDefinitions, relation1, relation2, ..., relationN)`
///
/// In order to add more edge definitions to the graph before creating
/// this function can be used to add more definitions to the initial list.
///
/// @PARAMS
///
/// @PARAM{edgeDefinitions, array, required}
/// A list of relation definition objects.
///
/// @PARAM{relationX, object, required}
/// An object representing a definition of one relation in the graph
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeDefinitionsExtend}
///   var graph_module = require("org/arangodb/general-graph");
///   directed_relation = graph_module._relation("lives_in", "user", "city");
///   undirected_relation = graph_module._relation("knows", "user", "user");
///   edgedefinitions = graph_module._edgeDefinitions(directed_relation);
///   edgedefinitions = graph_module._extendEdgeDefinitions(undirected_relation);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

var _extendEdgeDefinitions = function (edgeDefinition) {
  var args = arguments, i = 0;

  Object.keys(args).forEach(
    function (x) {
      i++;
      if (i === 1) {
        return;
      }
      edgeDefinition.push(args[x]);
    }
  );
};
////////////////////////////////////////////////////////////////////////////////
/// internal helper to sort a graph's edge definitions
////////////////////////////////////////////////////////////////////////////////
var sortEdgeDefinition = function(edgeDefinition) {
  edgeDefinition.from = edgeDefinition.from.sort();
  edgeDefinition.to = edgeDefinition.to.sort();
  return edgeDefinition;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new graph
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_how_to_create
///
/// * Create a graph
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo1}
///   var graph_module = require("org/arangodb/general-graph");
///   var graph = graph_module._create("myGraph");
///   graph;
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// * Add some vertex collections
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo2}
/// ~ var graph_module = require("org/arangodb/general-graph");
/// ~ var graph = graph_module._create("myGraph");
///   graph._addVertexCollection("shop");
///   graph._addVertexCollection("customer");
///   graph._addVertexCollection("pet");
///   graph;
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// * Define relations on the
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo3}
/// ~ var graph_module = require("org/arangodb/general-graph");
/// ~ var graph = graph_module._create("myGraph");
///   var rel = graph_module._relation("isCustomer", ["shop"], ["customer"]);
///   graph._extendEdgeDefinitions(rel);
///   graph;
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_create
/// @brief Create a graph
///
/// `graph_module._create(graphName, edgeDefinitions, orphanCollections)`
///
/// The creation of a graph requires the name of the graph and a definition of its edges.
///
/// For every type of edge definition a convenience method exists that can be used to create a graph.
/// Optionally a list of vertex collections can be added, which are not used in any edge definition.
/// These collections are referred to as orphan collections within this chapter.
/// All collections used within the creation process are created if they do not exist.
///
/// @PARAMS
///
/// @PARAM{graphName, string, required}
/// Unique identifier of the graph
///
/// @PARAM{edgeDefinitions, array, optional}
/// List of relation definition objects
///
/// @PARAM{orphanCollections, array, optional}
/// List of additional vertex collection names
///
/// @EXAMPLES
///
/// Create an empty graph, edge definitions can be added at runtime:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraph}
///   var graph_module = require("org/arangodb/general-graph");
///   graph = graph_module._create("myGraph");
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Create a graph with edge definitions and orphan collections:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraph2}
///   var graph_module = require("org/arangodb/general-graph");
/// | graph = graph_module._create("myGraph",
///   [graph_module._relation("myRelation", ["male", "female"])], ["sessions"]);
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

var _create = function (graphName, edgeDefinitions, orphanCollections, options) {

  if (! Array.isArray(orphanCollections) ) {
    orphanCollections = [];
  }
  var gdb = getGraphCollection(),
    err,
    graphAlreadyExists = true,
    collections,
    result;
  if (!graphName) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_NAME.message;
    throw err;
  }
  edgeDefinitions = edgeDefinitions || [];
  if (!Array.isArray(edgeDefinitions)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.message;
    throw err;
  }
  //check, if a collection is already used in a different edgeDefinition
  var tmpCollections = [];
  var tmpEdgeDefinitions = {};
  edgeDefinitions.forEach(
    function(edgeDefinition) {
      var col = edgeDefinition.collection;
      if (tmpCollections.indexOf(col) !== -1) {
        err = new ArangoError();
        err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.code;
        err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.message;
        throw err;
      }
      tmpCollections.push(col);
      tmpEdgeDefinitions[col] = edgeDefinition;
    }
  );
  gdb.toArray().forEach(
    function(singleGraph) {
      var sGEDs = singleGraph.edgeDefinitions;
      sGEDs.forEach(
        function(sGED) {
          var col = sGED.collection;
          if (tmpCollections.indexOf(col) !== -1) {
            if (JSON.stringify(sGED) !== JSON.stringify(tmpEdgeDefinitions[col])) {
              err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code;
              err.errorMessage = col + " "
                + arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message;
              throw err;
            }
          }
        }
      );
    }
  );

  try {
    gdb.document(graphName);
  } catch (e) {
    if (e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw e;
    }
    graphAlreadyExists = false;
  }

  if (graphAlreadyExists) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_DUPLICATE.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_DUPLICATE.message;
    throw err;
  }

  collections = findOrCreateCollectionsByEdgeDefinitions(edgeDefinitions, false);
  orphanCollections.forEach(
    function(oC) {
      findOrCreateCollectionByName(oC, ArangoCollection.TYPE_DOCUMENT);
    }
  );

  edgeDefinitions.forEach(
    function(eD, index) {
      var tmp = sortEdgeDefinition(eD);
      edgeDefinitions[index] = tmp;
    }
  );
  orphanCollections = orphanCollections.sort();

  var data =  gdb.save({
    'orphanCollections' : orphanCollections,
    'edgeDefinitions' : edgeDefinitions,
    '_key' : graphName
  }, options);

  result = new Graph(graphName, edgeDefinitions, collections[0], collections[1],
    orphanCollections, data._rev , data._id);
  return result;

};

var createHiddenProperty = function(obj, name, value) {
  Object.defineProperty(obj, name, {
    enumerable: false,
    writable: true
  });
  obj[name] = value;
};
////////////////////////////////////////////////////////////////////////////////
/// @brief helper for updating binded collections
////////////////////////////////////////////////////////////////////////////////
var removeEdge = function (graphs, edgeCollection, edgeId, self) {
  self.__idsToRemove[edgeId] = 1;
  graphs.forEach(
    function(graph) {
      var edgeDefinitions = graph.edgeDefinitions;
      if (graph.edgeDefinitions) {
        edgeDefinitions.forEach(
          function(edgeDefinition) {
            var from = edgeDefinition.from;
            var to = edgeDefinition.to;
            var collection = edgeDefinition.collection;
            // if collection of edge to be deleted is in from or to
            if (from.indexOf(edgeCollection) !== -1 || to.indexOf(edgeCollection) !== -1) {
              //search all edges of the graph
              var edges = db._collection(collection).edges(edgeId);
              edges.forEach(function(edge) {
                // if from is
                if(! self.__idsToRemove.hasOwnProperty(edge._id)) {
                  self.__collectionsToLock[collection] = 1;
                  removeEdge(graphs, collection, edge._id, self);
                }
              });
            }
          }
        );
      }
    }
  );
};

var bindEdgeCollections = function(self, edgeCollections) {
  _.each(edgeCollections, function(key) {
    var obj = db._collection(key);
    var wrap = wrapCollection(obj);
    // save
    var old_save = wrap.save;
    wrap.save = function(from, to, data) {
      if (typeof from !== 'string' || 
          from.indexOf('/') === -1 ||
          typeof to !== 'string' ||
          to.indexOf('/') === -1) {
        // invalid from or to value
        var err = new ArangoError();
        err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
        err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
        throw err;
      }

      //check, if edge is allowed
      self.__edgeDefinitions.forEach(
        function(edgeDefinition) {
          if (edgeDefinition.collection === key) {
            var fromCollection = from.split("/")[0];
            var toCollection = to.split("/")[0];
            if (! _.contains(edgeDefinition.from, fromCollection)
              || ! _.contains(edgeDefinition.to, toCollection)) {
              var err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_EDGE.code;
              err.errorMessage =
                arangodb.errors.ERROR_GRAPH_INVALID_EDGE.message + " between " + from + " and " + to + ".";
              throw err;
            }
          }
        }
      );
      return old_save(from, to, data);
    };

    // remove
    wrap.remove = function(edgeId, options) {
      //if _key make _id (only on 1st call)
      if (edgeId.indexOf("/") === -1) {
        edgeId = key + "/" + edgeId;
      }
      var graphs = getGraphCollection().toArray();
      var edgeCollection = edgeId.split("/")[0];
      self.__collectionsToLock[edgeCollection] = 1;
      removeEdge(graphs, edgeCollection, edgeId, self);

      try {
        db._executeTransaction({
          collections: {
            write: Object.keys(self.__collectionsToLock)
          },
          embed: true,
          action: function (params) {
            var db = require("internal").db;
            params.ids.forEach(
              function(edgeId) {
                if (params.options) {
                  db._remove(edgeId, params.options);
                } else {
                  db._remove(edgeId);
                }
              }
            );
          },
          params: {
            ids: Object.keys(self.__idsToRemove),
            options: options
          }
        });
      } catch (e) {
        self.__idsToRemove = {};
        self.__collectionsToLock = {};
        throw e;
      }
      self.__idsToRemove = {};
      self.__collectionsToLock = {};

      return true;
    };

    self[key] = wrap;
  });
};

var bindVertexCollections = function(self, vertexCollections) {
  _.each(vertexCollections, function(key) {
    var obj = db._collection(key);
    var wrap = wrapCollection(obj);
    wrap.remove = function(vertexId, options) {
      //delete all edges using the vertex in all graphs
      var graphs = getGraphCollection().toArray();
      var vertexCollectionName = key;
      if (vertexId.indexOf("/") === -1) {
        vertexId = key + "/" + vertexId;
      }
      self.__collectionsToLock[vertexCollectionName] = 1;
      graphs.forEach(
        function(graph) {
          var edgeDefinitions = graph.edgeDefinitions;
          if (graph.edgeDefinitions) {
            edgeDefinitions.forEach(
              function(edgeDefinition) {
                var from = edgeDefinition.from;
                var to = edgeDefinition.to;
                var collection = edgeDefinition.collection;
                if (from.indexOf(vertexCollectionName) !== -1
                  || to.indexOf(vertexCollectionName) !== -1
                  ) {
                  var edges = db._collection(collection).edges(vertexId);
                  if (edges.length > 0) {
                    self.__collectionsToLock[collection] = 1;
                    edges.forEach(function(edge) {
                      removeEdge(graphs, collection, edge._id, self);
                    });
                  }
                }
              }
            );
          }
        }
      );

      try {
        db._executeTransaction({
          collections: {
            write: Object.keys(self.__collectionsToLock)
          },
          embed: true,
          action: function (params) {
            var db = require("internal").db;
            params.ids.forEach(
              function(edgeId) {
                if (params.options) {
                  db._remove(edgeId, params.options);
                } else {
                  db._remove(edgeId);
                }
              }
            );
            if (params.options) {
              db._remove(params.vertexId, params.options);
            } else {
              db._remove(params.vertexId);
            }
          },
          params: {
            ids: Object.keys(self.__idsToRemove),
            options: options,
            vertexId: vertexId
          }
        });
      } catch (e) {
        self.__idsToRemove = {};
        self.__collectionsToLock = {};
        throw e;
      }
      self.__idsToRemove = {};
      self.__collectionsToLock = {};

      return true;
    };
    self[key] = wrap;
  });

};
var updateBindCollections = function(graph) {
  //remove all binded collections
  Object.keys(graph).forEach(
    function(key) {
      if(key.substring(0,1) !== "_") {
        delete graph[key];
      }
    }
  );
  graph.__edgeDefinitions.forEach(
    function(edgeDef) {
      bindEdgeCollections(graph, [edgeDef.collection]);
      bindVertexCollections(graph, edgeDef.from);
      bindVertexCollections(graph, edgeDef.to);
    }
  );
  bindVertexCollections(graph, graph.__orphanCollections);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_save
/// @brief Create a new vertex in vertexCollectionName
///
/// `graph.vertexCollectionName.save(data)`
///
/// @PARAMS
///
/// @PARAM{data, object, required}
/// JSON data of vertex.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionSave}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.male.save({name: "Floyd", _key: "floyd"});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_replace
/// @brief Replaces the data of a vertex in collection vertexCollectionName
///
/// `graph.vertexCollectionName.replace(vertexId, data, options)`
///
/// @PARAMS
///
/// @PARAM{vertexId, string, required}
/// *_id* attribute of the vertex
///
/// @PARAM{data, object, required}
/// JSON data of vertex.
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionReplace}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.male.save({neym: "Jon", _key: "john"});
///   graph.male.replace("male/john", {name: "John"});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_update
/// @brief Updates the data of a vertex in collection vertexCollectionName
///
/// `graph.vertexCollectionName.update(vertexId, data, options)`
///
/// @PARAMS
///
/// @PARAM{vertexId, string, required}
/// *_id* attribute of the vertex
///
/// @PARAM{data, object, required}
/// JSON data of vertex.
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionUpdate}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.female.save({name: "Lynda", _key: "linda"});
///   graph.female.update("female/linda", {name: "Linda", _key: "linda"});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_remove
/// @brief Removes a vertex in collection *vertexCollectionName*
///
/// `graph.vertexCollectionName.remove(vertexId, options)`
///
/// Additionally removes all ingoing and outgoing edges of the vertex recursively
/// (see [edge remove](#edge.remove)).
///
/// @PARAMS
///
/// @PARAM{vertexId, string, required}
/// *_id* attribute of the vertex
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionRemove}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.male.save({name: "Kermit", _key: "kermit"});
///   db._exists("male/kermit")
///   graph.male.remove("male/kermit")
///   db._exists("male/kermit")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_save
/// @brief Creates an edge from vertex *from* to vertex *to* in collection edgeCollectionName
///
/// `graph.edgeCollectionName.save(from, to, data, options)`
///
/// @PARAMS
///
/// @PARAM{from, string, required}
/// *_id* attribute of the source vertex
///
/// @PARAM{to, string, required}
/// *_id* attribute of the target vertex
///
/// @PARAM{data, object, required}
/// JSON data of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Edges/README.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionSave1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("male/bob", "female/alice", {type: "married", _key: "bobAndAlice"});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// If the collections of *from* and *to* are not defined in an edge definition of the graph,
/// the edge will not be stored.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionSave2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("relation/aliceAndBob", "female/alice", {type: "married", _key: "bobAndAlice"});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_replace
/// @brief Replaces the data of an edge in collection edgeCollectionName
///
/// `graph.edgeCollectionName.replace(edgeId, data, options)`
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @PARAM{data, object, required}
/// JSON data of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionReplace}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("female/alice", "female/diana", {typo: "nose", _key: "aliceAndDiana"});
///   graph.relation.replace("relation/aliceAndDiana", {type: "knows"});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_update
/// @brief Updates the data of an edge in collection edgeCollectionName
///
/// `graph.edgeCollectionName.update(edgeId, data, options)`
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @PARAM{data, object, required}
/// JSON data of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionUpdate}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("female/alice", "female/diana", {type: "knows", _key: "aliceAndDiana"});
///   graph.relation.update("relation/aliceAndDiana", {type: "quarrelled", _key: "aliceAndDiana"});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_remove
/// @brief Removes an edge in collection edgeCollectionName
///
/// `graph.edgeCollectionName.remove(edgeId, options)`
///
/// If this edge is used as a vertex by another edge, the other edge will be removed (recursively).
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionRemove}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("female/alice", "female/diana", {_key: "aliceAndDiana"});
///   db._exists("relation/aliceAndDiana")
///   graph.relation.remove("relation/aliceAndDiana")
///   db._exists("relation/aliceAndDiana")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
var Graph = function(graphName, edgeDefinitions, vertexCollections, edgeCollections,
                     orphanCollections, revision, id) {
  edgeDefinitions.forEach(
    function(eD, index) {
      var tmp = sortEdgeDefinition(eD);
      edgeDefinitions[index] = tmp;
    }
  );

  if (!orphanCollections) {
    orphanCollections = [];
  }

  // we can call the "fast" version of some edge functions if we are
  // running server-side and are not a coordinator
  var useBuiltIn = (typeof ArangoClusterComm === "object");
  if (useBuiltIn && require("org/arangodb/cluster").isCoordinator()) {
    useBuiltIn = false;
  }

  var self = this;
  // Create Hidden Properties
  createHiddenProperty(this, "__useBuiltIn", useBuiltIn);
  createHiddenProperty(this, "__name", graphName);
  createHiddenProperty(this, "__vertexCollections", vertexCollections);
  createHiddenProperty(this, "__edgeCollections", edgeCollections);
  createHiddenProperty(this, "__edgeDefinitions", edgeDefinitions);
  createHiddenProperty(this, "__idsToRemove", {});
  createHiddenProperty(this, "__collectionsToLock", {});
  createHiddenProperty(this, "__id", id);
  createHiddenProperty(this, "__rev", revision);
  createHiddenProperty(this, "__orphanCollections", orphanCollections);
  updateBindCollections(self);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_graph
/// @brief Get a graph
///
/// `graph_module._graph(graphName)`
///
/// A graph can be get by its name.
///
/// @PARAMS
///
/// @PARAM{graphName, string, required}
/// Unique identifier of the graph
///
/// @EXAMPLES
///
/// Get a graph:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphLoadGraph}
/// ~ var examples = require("org/arangodb/graph-examples/example-graph.js");
/// ~ var g1 = examples.loadGraph("social");
///   var graph_module = require("org/arangodb/general-graph");
///   graph = graph_module._graph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

var _graph = function(graphName) {

  var gdb = getGraphCollection(),
    g, collections, orphanCollections;

  try {
    g = gdb.document(graphName);
  }
  catch (e) {
    if (e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw e;
    }
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message;
    throw err;
  }

  collections = findOrCreateCollectionsByEdgeDefinitions(g.edgeDefinitions, true);
  orphanCollections = g.orphanCollections;
  if (!orphanCollections) {
    orphanCollections = [];
  }

  return new Graph(graphName, g.edgeDefinitions, collections[0], collections[1], orphanCollections,
    g._rev , g._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a graph exists.
////////////////////////////////////////////////////////////////////////////////

var _exists = function(graphId) {
  var gCol = getGraphCollection();
  return gCol.exists(graphId);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper for dropping collections of a graph.
////////////////////////////////////////////////////////////////////////////////

var checkIfMayBeDropped = function(colName, graphName, graphs) {
  var result = true;
  graphs.forEach(
    function(graph) {
      if (graph._key === graphName) {
        return;
      }
      var edgeDefinitions = graph.edgeDefinitions;
      if (edgeDefinitions) {
        edgeDefinitions.forEach(
          function(edgeDefinition) {
            var from = edgeDefinition.from;
            var to = edgeDefinition.to;
            var collection = edgeDefinition.collection;
            if (collection === colName
              || from.indexOf(colName) !== -1
              || to.indexOf(colName) !== -1
              ) {
              result = false;
            }
          }
        );
      }

      var orphanCollections = graph.orphanCollections;
      if (orphanCollections) {
        if (orphanCollections.indexOf(colName) !== -1) {
          result = false;
        }
      }
    }
  );

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_drop
/// @brief Remove a graph
///
/// `graph_module._drop(graphName, dropCollections)`
///
/// A graph can be dropped by its name.
/// This will automatically drop all collections contained in the graph as
/// long as they are not used within other graphs.
/// To drop the collections, the optional parameter *drop-collections* can be set to *true*.
///
/// @PARAMS
///
/// @PARAM{graphName, string, required}
/// Unique identifier of the graph
///
/// @PARAM{dropCollections, boolean, optional}
/// Define if collections should be dropped (default: false)
///
/// @EXAMPLES
///
/// Drop a graph and keep collections:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDropGraphKeep}
/// ~ var examples = require("org/arangodb/graph-examples/example-graph.js");
/// ~ var g1 = examples.loadGraph("social");
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._drop("social");
///   db._collection("female");
///   db._collection("male");
///   db._collection("relation");
/// ~ db._drop("female");
/// ~ db._drop("male");
/// ~ db._drop("relation");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDropGraphDropCollections}
/// ~ var examples = require("org/arangodb/graph-examples/example-graph.js");
/// ~ var g1 = examples.loadGraph("social");
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._drop("social", true);
///   db._collection("female");
///   db._collection("male");
///   db._collection("relation");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

var _drop = function(graphId, dropCollections) {

  var gdb = getGraphCollection(),
    graphs;

  if (!gdb.exists(graphId)) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message;
    throw err;
  }

  if (dropCollections === true) {
    var graph = gdb.document(graphId);
    var edgeDefinitions = graph.edgeDefinitions;
    edgeDefinitions.forEach(
      function(edgeDefinition) {
        var from = edgeDefinition.from;
        var to = edgeDefinition.to;
        var collection = edgeDefinition.collection;
        graphs = getGraphCollection().toArray();
        if (checkIfMayBeDropped(collection, graph._key, graphs)) {
          db._drop(collection);
        }
        from.forEach(
          function(col) {
            if (checkIfMayBeDropped(col, graph._key, graphs)) {
              db._drop(col);
            }
          }
        );
        to.forEach(
          function(col) {
            if (checkIfMayBeDropped(col, graph._key, graphs)) {
              db._drop(col);
            }
          }
        );
      }
    );
    //drop orphans
    graphs = getGraphCollection().toArray();
    if (!graph.orphanCollections) {
      graph.orphanCollections = [];
    }
    graph.orphanCollections.forEach(
      function(oC) {
        if (checkIfMayBeDropped(oC, graph._key, graphs)) {
          try {
            db._drop(oC);
          } catch (ignore) {}
        }
      }
    );
  }

  gdb.remove(graphId);
  return true;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief return all edge collections of the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._edgeCollections = function() {
  return _.values(this.__edgeCollections);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return all vertex collections of the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._vertexCollections = function() {
  var orphans = [];
  _.each(this.__orphanCollections, function(o) {
    orphans.push(db[o]);
  });
  return _.union(_.values(this.__vertexCollections), orphans);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _EDGES(vertexId).
////////////////////////////////////////////////////////////////////////////////

// might be needed from AQL itself
Graph.prototype._EDGES = function(vertexId) {
  var err;
  if (vertexId.indexOf("/") === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ": " + vertexId;
    throw err;
  }

  var result = [], c;
  for (c in this.__edgeCollections) {
    if (this.__edgeCollections.hasOwnProperty(c)) {
      if (this.__useBuiltIn) {
        result = result.concat(this.__edgeCollections[c].EDGES(vertexId));
      }
      else {
        result = result.concat(this.__edgeCollections[c].edges(vertexId));
      }
    }
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief INEDGES(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._INEDGES = function(vertexId) {
  var err;
  if (vertexId.indexOf("/") === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ": " + vertexId;
    throw err;
  }

  var result = [], c;
  for (c in this.__edgeCollections) {
    if (this.__edgeCollections.hasOwnProperty(c)) {
      if (this.__useBuiltIn) {
        result = result.concat(this.__edgeCollections[c].INEDGES(vertexId));
      }
      else {
        result = result.concat(this.__edgeCollections[c].inEdges(vertexId));
      }
    }
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._OUTEDGES = function(vertexId) {
  var err;
  if (vertexId.indexOf("/") === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ": " + vertexId;
    throw err;
  }

  var result = [], c;
  for (c in this.__edgeCollections) {
    if (this.__edgeCollections.hasOwnProperty(c)) {
      if (this.__useBuiltIn) {
        result = result.concat(this.__edgeCollections[c].OUTEDGES(vertexId));
      }
      else {
        result = result.concat(this.__edgeCollections[c].outEdges(vertexId));
      }
    }
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edges
/// @brief Select some edges from the graph.
///
/// `graph._edges(examples)`
///
/// Creates an AQL statement to select a subset of the edges stored in the graph.
/// This is one of the entry points for the fluent AQL interface.
/// It will return a mutable AQL statement which can be further refined, using the
/// functions described below.
/// The resulting set of edges can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// In the examples the *toArray* function is used to print the result.
/// The description of this function can be found below.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._edges().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgesFiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._edges({type: "married"}).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._edges = function(edgeExample) {
  var AQLStmt = new AQLGenerator(this);
  // If no direction is specified all edges are duplicated.
  // => For initial requests a direction has to be set
  return AQLStmt.outEdges(edgeExample);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertices
/// @brief Select some vertices from the graph.
///
/// `graph._vertices(examples)`
///
/// Creates an AQL statement to select a subset of the vertices stored in the graph.
/// This is one of the entry points for the fluent AQL interface.
/// It will return a mutable AQL statement which can be further refined, using the
/// functions described below.
/// The resulting set of edges can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// In the examples the *toArray* function is used to print the result.
/// The description of this function can be found below.
///
/// To request unfiltered vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVerticesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._vertices().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVerticesFiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._vertices([{name: "Alice"}, {name: "Bob"}]).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._vertices = function(example) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.vertices(example);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fromVertex
/// @brief Get the source vertex of an edge
///
/// `graph._fromVertex(edgeId)`
///
/// Returns the vertex defined with the attribute *_from* of the edge with *edgeId* as its *_id*.
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphGetFromVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._fromVertex("relation/aliceAndBob")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._fromVertex = function(edgeId) {
  if (typeof edgeId !== 'string' ||
      edgeId.indexOf('/') === -1) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
    throw err;
  }
  var edgeCollection = this._getEdgeCollectionByName(edgeId.split("/")[0]);
  var document = edgeCollection.document(edgeId);
  if (document) {
    var vertexId = document._from;
    var vertexCollection = this._getVertexCollectionByName(vertexId.split("/")[0]);
    return vertexCollection.document(vertexId);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_toVertex
/// @brief Get the target vertex of an edge
///
/// `graph._toVertex(edgeId)`
///
/// Returns the vertex defined with the attribute *_to* of the edge with *edgeId* as its *_id*.
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphGetToVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._toVertex("relation/aliceAndBob")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._toVertex = function(edgeId) {
  if (typeof edgeId !== 'string' ||
      edgeId.indexOf('/') === -1) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
    throw err;
  }
  var edgeCollection = this._getEdgeCollectionByName(edgeId.split("/")[0]);
  var document = edgeCollection.document(edgeId);
  if (document) {
    var vertexId = document._to;
    var vertexCollection = this._getVertexCollectionByName(vertexId.split("/")[0]);
    return vertexCollection.document(vertexId);
  }
};


////////////////////////////////////////////////////////////////////////////////
/// @brief get edge collection by name.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getEdgeCollectionByName = function(name) {
  if (this.__edgeCollections[name]) {
    return this.__edgeCollections[name];
  }
  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
  err.errorMessage = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message + ": " + name;
  throw err;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get vertex collection by name.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getVertexCollectionByName = function(name) {
  if (this.__vertexCollections[name]) {
    return this.__vertexCollections[name];
  }
  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
  err.errorMessage = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message + ": " + name;
  throw err;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_neighbors
/// @brief Get all neighbors of the vertices defined by the example
///
/// `graph._neighbors(vertexExample, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for vertexExample.
/// The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
/// parameter vertexExamplex, *m* the average amount of neighbors and *x* the maximal depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// @PARAMS
///
/// @PARAM{vertexExample, object, optional}
/// See [Definition of examples](#definition_of_examples)
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeExamples*: Filter the edges, see [Definition of examples](#definition_of_examples)
///   * *neighborExamples*: Filter the neighbor vertices, see [Definition of examples](#definition_of_examples)
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *vertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered on the intermediate vertex steps.
///   * *minDepth*: Defines the minimal number of intermediate steps to neighbors (default is 1).
///   * *maxDepth*: Defines the maximal number of intermediate steps to neighbors (default is 1).
///
/// @EXAMPLES
///
/// A route planner example, all neighbors of capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleNeighbors1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._neighbors({isCapital : true});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all outbound neighbors of Hamburg.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleNeighbors2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._neighbors('germanCity/Hamburg', {direction : 'outbound', maxDepth : 2});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._neighbors = function(vertexExample, options) {
  var AQLStmt = new AQLGenerator(this);
  // If no direction is specified all edges are duplicated.
  // => For initial requests a direction has to be set
  if (!options) {
    options = {};
  }
  return AQLStmt.vertices(vertexExample).neighbors(options.neighborExamples, options)
    .toArray();
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_common_neighbors
/// @brief Get all common neighbors of the vertices defined by the examples.
///
/// `graph._commonNeighbors(vertex1Example, vertex2Examples, optionsVertex1, optionsVertex2)`
///
/// This function returns the intersection of *graph_module._neighbors(vertex1Example, optionsVertex1)*
/// and *graph_module._neighbors(vertex2Example, optionsVertex2)*.
/// For parameter documentation see [_neighbors](#_neighbors).
///
/// The complexity of this method is **O(n\*m^x)** with *n* being the maximal amount of vertices
/// defined by the parameters vertexExamples, *m* the average amount of neighbors and *x* the
/// maximal depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// @EXAMPLES
///
/// A route planner example, all common neighbors of capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighbors1}
/// ~ var db = require("internal").db;
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._commonNeighbors({isCapital : true}, {isCapital : true});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all common outbound neighbors of Hamburg with any other location
/// which have a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighbors2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | graph._commonNeighbors(
/// |   'germanCity/Hamburg',
/// |   {},
/// |   {direction : 'outbound', maxDepth : 2},
///     {direction : 'outbound', maxDepth : 2});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._commonNeighbors = function(vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {

  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  optionsVertex1 = optionsVertex1 || {};
  optionsVertex2 = optionsVertex2 || {};
  var bindVars = {
    "graphName": this.__name,
    "options1": optionsVertex1,
    "options2": optionsVertex2,
    "ex1": ex1,
    "ex2": ex2
  };
  return db._query(COMMON_NEIGHBORS, bindVars, {count: true}).toArray();
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_count_common_neighbors
/// @brief Get the amount of common neighbors of the vertices defined by the examples.
///
/// `graph._countCommonNeighbors(vertex1Example, vertex2Examples, optionsVertex1, optionsVertex2)`
///
/// Similar to [_commonNeighbors](#_commonNeighbors) but returns count instead of the elements.
///
/// @EXAMPLES
///
/// A route planner example, all common neighbors of capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighborsAmount1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._countCommonNeighbors({isCapital : true}, {isCapital : true});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all common outbound neighbors of Hamburg with any other location
/// which have a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighborsAmount2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | graph._countCommonNeighbors('germanCity/Hamburg', {}, {direction : 'outbound', maxDepth : 2},
///   {direction : 'outbound', maxDepth : 2});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._countCommonNeighbors = function(vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {
  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  optionsVertex1 = optionsVertex1 || {};
  optionsVertex2 = optionsVertex2 || {};
  var bindVars = {
    "graphName": this.__name,
    "options1": optionsVertex1,
    "options2": optionsVertex2,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(COUNT_COMMON_NEIGHBORS, bindVars, {count: true}).toArray(),
    tmp = {}, tmp2={}, returnHash = [];
  result.forEach(function (r) {
    if (!tmp[r[0]]) {
      tmp[r[0]] = [];
    }
    tmp2 = {};
    tmp2[r[1]] = r[2];
    tmp[r[0]].push(tmp2);
  });
  Object.keys(tmp).forEach(function(w) {
    tmp2 = {};
    tmp2[w] = tmp[w];
    returnHash.push(tmp2);
  });
  return returnHash;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_common_properties
/// @brief Get the vertices of the graph that share common properties.
///
/// `graph._commonProperties(vertex1Example, vertex2Examples, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for vertex1Example and vertex2Example.
///
/// The complexity of this method is **O(n)** with *n* being the maximal amount of vertices
/// defined by the parameters vertexExamples.
///
/// @PARAMS
///
/// @PARAM{vertex1Examples, object, optional}
/// Filter the set of source vertices, see [Definition of examples](#definition_of_examples)
///
/// @PARAM{vertex2Examples, object, optional}
/// Filter the set of vertices compared to, see [Definition of examples](#definition_of_examples)
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *vertex1CollectionRestriction* : One or a list of vertex-collection names that should be
///       searched for source vertices.
///   * *vertex2CollectionRestriction* : One or a list of vertex-collection names that should be
///       searched for compare vertices.
///   * *ignoreProperties* : One or a list of attribute names of a document that should be ignored.
///
/// @EXAMPLES
///
/// A route planner example, all locations with the same properties:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleProperties1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._commonProperties({}, {});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all cities which share same properties except for population.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleProperties2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._commonProperties({}, {}, {ignoreProperties: 'population'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._commonProperties = function(vertex1Example, vertex2Example, options) {
  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  return db._query(COMMON_PROPERTIES, bindVars, {count: true}).toArray();
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_count_common_properties
/// @brief Get the amount of vertices of the graph that share common properties.
///
/// `graph._countCommonProperties(vertex1Example, vertex2Examples, options)`
///
/// Similar to [_commonProperties](#_commonProperties) but returns count instead of
/// the objects.
///
/// @EXAMPLES
///
/// A route planner example, all locations with the same properties:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAmountProperties1}
/// ~ var db = require("internal").db;
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._countCommonProperties({}, {});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all German cities which share same properties except for population.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAmountProperties2}
/// ~ var db = require("internal").db;
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// | graph._countCommonProperties({}, {}, {vertex1CollectionRestriction : 'germanCity',
///   vertex2CollectionRestriction : 'germanCity' ,ignoreProperties: 'population'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._countCommonProperties = function(vertex1Example, vertex2Example, options) {
  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(COUNT_COMMON_PROPERTIES, bindVars, {count: true}).toArray(), returnHash = [];
  result.forEach(function (r) {
    var tmp = {};
    tmp[r[0]] = r[1];
    returnHash.push(tmp);
  });
  return returnHash;
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_paths
/// @brief The _paths function returns all paths of a graph.
///
/// `graph._paths(options)`
///
/// This function determines all available paths in a graph.
///
/// The complexity of this method is **O(n\*n\*m)** with *n* being the amount of vertices in
/// the graph and *m* the average amount of connected edges;
///
/// @PARAMS
///
/// @PARAM{options, object, optional}
/// An object containing options, see below:
///   * *direction*        : The direction of the edges. Possible values are *any*,
///     *inbound* and *outbound* (default).
///   * *followCycles* (optional) : If set to *true* the query follows cycles in the graph,
///     default is false.
///   * *minLength* (optional)     : Defines the minimal length a path must
///     have to be returned (default is 0).
///   * *maxLength* (optional)     : Defines the maximal length a path must
///      have to be returned (default is 10).
///
/// @EXAMPLES
///
/// Return all paths of the graph "social":
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModulePaths}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   g._paths();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Return all inbound paths of the graph "social" with a maximal
/// length of 1 and a minimal length of 2:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModulePaths2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   g._paths({direction : 'inbound', minLength : 1, maxLength :  2});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._paths = function(options) {
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(PATHS, bindVars).toArray();
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_shortest_path
/// @brief The _shortestPath function returns all shortest paths of a graph.
///
/// `graph._shortestPath(startVertexExample, endVertexExample, options)`
///
/// This function determines all shortest paths in a graph.
/// The function accepts an id, an example, a list of examples
/// or even an empty example as parameter for
/// start and end vertex. If one wants to call this function to receive nearly all
/// shortest paths for a graph the option *algorithm* should be set to
/// [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
/// to increase performance.
/// If no algorithm is provided in the options the function chooses the appropriate
/// one (either [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
/// or [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)) according to its parameters.
/// The length of a path is by default the amount of edges from one start vertex to
/// an end vertex. The option weight allows the user to define an edge attribute
/// representing the length.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{startVertexExample, object, optional}
/// An example for the desired start Vertices
/// (see [Definition of examples](#definition_of_examples)).
///
/// @PARAM{endVertexExample, object, optional}
/// An example for the desired
/// end Vertices (see [Definition of examples](#definition_of_examples)).
///
/// @PARAM{options, object, optional}
/// An object containing options, see below:
///   * *direction*                        : The direction of the edges as a string.
///   Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *startVertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   start vertex of a path.
///   * *endVertexCollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   end vertex of a path.
///   * *edgeExamples*                     : A filter example for the
///   edges in the shortest paths
///   (see [example](#short_explaination_of_the_vertex_example_parameter)).
///   * *algorithm*                        : The algorithm to calculate
///   the shortest paths. If both start and end vertex examples are empty
///   [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) is
///   used, otherwise the default is [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)
///   * *weight*                           : The name of the attribute of
///   the edges containing the length as a string.
///   * *defaultWeight*                    : Only used with the option *weight*.
///   If an edge does not have the attribute named as defined in option *weight* this default
///   is used as length.
///   If no default is supplied the default would be positive Infinity so the path could
///   not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, shortest path from all german to all french cities:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleShortestPaths1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | g._shortestPath({}, {}, {weight : 'distance', endVertexCollectionRestriction : 'frenchCity',
///   startVertexCollectionRestriction : 'germanCity'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, shortest path from Hamburg and Cologne to Lyon:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleShortestPaths2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | g._shortestPath([{_id: 'germanCity/Cologne'},{_id: 'germanCity/Munich'}], 'frenchCity/Lyon',
///   {weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._shortestPath = function(startVertexExample, endVertexExample, options) {
  var ex1 = transformExample(startVertexExample);
  var ex2 = transformExample(endVertexExample);
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(SHORTEST_PATH, bindVars).toArray();
  return result;
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_distance_to
/// @brief The _distanceTo function returns all paths and there distance within a graph.
///
/// `graph._distanceTo(startVertexExample, endVertexExample, options)`
///
/// This function is a wrapper of [graph._shortestPath](#_shortestpath).
/// It does not return the actual path but only the distance between two vertices.
///
/// @EXAMPLES
///
/// A route planner example, shortest distance from all german to all french cities:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDistanceTo1}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | g._distanceTo({}, {}, {weight : 'distance', endVertexCollectionRestriction : 'frenchCity',
///   startVertexCollectionRestriction : 'germanCity'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, shortest distance from Hamburg and Cologne to Lyon:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDistanceTo2}
/// ~ var db = require("internal").db;
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | g._distanceTo([{_id: 'germanCity/Cologne'},{_id: 'germanCity/Munich'}], 'frenchCity/Lyon',
///   {weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._distanceTo = function(startVertexExample, endVertexExample, options) {
  var ex1 = transformExample(startVertexExample);
  var ex2 = transformExample(endVertexExample);
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(DISTANCE_TO, bindVars).toArray();
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_absolute_eccentricity
/// @brief Get the
/// [eccentricity](http://en.wikipedia.org/wiki/Distance_%28graph_theory%29)
/// of the vertices defined by the examples.
///
/// `graph._absoluteEccentricity(vertexExample, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for vertexExample.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{vertexExample, object, optional}
/// Filter the vertices, see [Definition of examples](#definition_of_examples)
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *startVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for source vertices.
///   * *endVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for target vertices.
///   * *edgeExamples*: Filter the edges to be followed, see [Definition of examples](#definition_of_examples)
///   * *algorithm*: The algorithm to calculate the shortest paths, possible values are
///       [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
///       [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute eccentricity of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsEccentricity1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY("
/// |   + "'routeplanner', {})"
///   ).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsEccentricity2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteEccentricity({}, {weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsEccentricity3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | graph._absoluteEccentricity({}, {startVertexCollectionRestriction : 'germanCity',
///   direction : 'outbound', weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._absoluteEccentricity = function(vertexExample, options) {
  var ex1 = transformExample(vertexExample);
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1
  };
  var result = db._query(ABSOLUTE_ECCENTRICITY, bindVars).toArray();
  return result;
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_eccentricity
/// @brief Get the normalized
/// [eccentricity](http://en.wikipedia.org/wiki/Distance_%28graph_theory%29)
/// of the vertices defined by the examples.
///
/// `graph._eccentricity(vertexExample, options)`
///
/// Similar to [_absoluteEccentricity](#_absoluteeccentricity) but returns a normalized result.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @EXAMPLES
///
/// A route planner example, the eccentricity of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleEccentricity2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._eccentricity();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the weighted eccentricity.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleEccentricity3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._eccentricity({weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._eccentricity = function(options) {
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(ECCENTRICITY, bindVars).toArray();
  return result;
};



////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_absolute_closeness
/// @brief Get the
/// [closeness](http://en.wikipedia.org/wiki/Centrality#Closeness_centrality)
/// of the vertices defined by the examples.
///
/// `graph._absoluteCloseness(vertexExample, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for *vertexExample*.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{vertexExample, object, optional}
/// Filter the vertices, see [Definition of examples](#definition_of_examples)
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *startVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for source vertices.
///   * *endVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for target vertices.
///   * *edgeExamples*: Filter the edges to be followed, see [Definition of examples](#definition_of_examples)
///   * *algorithm*: The algorithm to calculate the shortest paths, possible values are
///       [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
///       [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the closeness can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute closeness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsCloseness1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteCloseness({});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsCloseness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteCloseness({}, {weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all German Cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsCloseness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | graph._absoluteCloseness({}, {startVertexCollectionRestriction : 'germanCity',
///   direction : 'outbound', weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._absoluteCloseness = function(vertexExample, options) {
  var ex1 = transformExample(vertexExample);
  var query = "RETURN"
    + " GRAPH_ABSOLUTE_CLOSENESS(@graphName"
    + ',@ex1'
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1
  };
  var result = db._query(query, bindVars).toArray();
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_closeness
/// @brief Get the normalized
/// [closeness](http://en.wikipedia.org/wiki/Centrality#Closeness_centrality)
/// of graphs vertices.
///
/// `graph._closeness(options)`
///
/// Similar to [_absoluteCloseness](#_absolutecloseness) but returns a normalized value.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @EXAMPLES
///
/// A route planner example, the normalized closeness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCloseness1}
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._closeness();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCloseness2}
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._closeness({weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the closeness of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCloseness3}
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._closeness({direction : 'outbound', weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._closeness = function(options) {
  var query = "RETURN"
    + " GRAPH_CLOSENESS(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  return result;
};



////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_absolute_betweenness
/// @brief Get the
/// [betweenness](http://en.wikipedia.org/wiki/Betweenness_centrality)
/// of all vertices in the graph.
///
/// `graph._absoluteBetweenness(options)`
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the betweeness can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute betweenness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsBetweenness1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteBetweenness({});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute betweenness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsBetweenness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteBetweenness({weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute betweenness of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsBetweenness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteBetweenness({direction : 'outbound', weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._absoluteBetweenness = function(options) {

  var query = "RETURN"
    + " GRAPH_ABSOLUTE_BETWEENNESS(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_betweenness
/// @brief Get the normalized
/// [betweenness](http://en.wikipedia.org/wiki/Betweenness_centrality)
/// of graphs vertices.
///
/// `graph_module._betweenness(options)`
///
/// Similar to [_absoluteBetweeness](#_absolutebetweeness) but returns normalized values.
///
/// @EXAMPLES
///
/// A route planner example, the betweenness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleBetweenness1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._betweenness();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the betweenness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleBetweenness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._betweenness({weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the betweenness of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleBetweenness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._betweenness({direction : 'outbound', weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._betweenness = function(options) {

  var query = "RETURN"
    + " GRAPH_BETWEENNESS(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  return result;
};



////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_radius
/// @brief Get the
/// [radius](http://en.wikipedia.org/wiki/Eccentricity_%28graph_theory%29)
/// of a graph.
///
/// `graph._radius(options)`
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *algorithm*: The algorithm to calculate the shortest paths, possible values are
///       [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
///       [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the radius can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the radius of the graph.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleRadius1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._radius();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the radius of the graph.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleRadius2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._radius({weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the radius of the graph regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleRadius3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._radius({direction : 'outbound', weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._radius = function(options) {

  var query = "RETURN"
    + " GRAPH_RADIUS(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  return result;
};



////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_diameter
/// @brief Get the
/// [diameter](http://en.wikipedia.org/wiki/Eccentricity_%28graph_theory%29)
/// of a graph.
///
/// `graph._diameter(graphName, options)`
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *algorithm*: The algorithm to calculate the shortest paths, possible values are
///       [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
///       [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the radius can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the diameter of the graph.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDiameter1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._diameter();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the diameter of the graph.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDiameter2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._diameter({weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the diameter of the graph regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDiameter3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._diameter({direction : 'outbound', weight : 'distance'});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._diameter = function(options) {

  var query = "RETURN"
    + " GRAPH_DIAMETER(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  return result;
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__extendEdgeDefinitions
/// @brief Add another edge definition to the graph
///
/// `graph._extendEdgeDefinitions(edgeDefinition)`
///
/// Extends the edge definitions of a graph. If an orphan collection is used in this
/// edge definition, it will be removed from the orphanage. If the edge collection of
/// the edge definition to add is already used in the graph or used in a different
/// graph with different *from* and/or *to* collections an error is thrown.
///
/// @PARAMS
///
/// @PARAM{edgeDefinition, object, required}
/// The relation definition to extend the graph
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__extendEdgeDefinitions}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._extendEdgeDefinitions(ed2);
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._extendEdgeDefinitions = function(edgeDefinition) {
  edgeDefinition = sortEdgeDefinition(edgeDefinition);
  var self = this;
  var err;
  //check if edgeCollection not already used
  var eC = edgeDefinition.collection;
  // ... in same graph
  if (this.__edgeCollections[eC] !== undefined) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.message;
    throw err;
  }
  var graphCol = getGraphCollection();
  //in different graph
  graphCol.toArray().forEach(
    function(singleGraph) {
      var sGEDs = singleGraph.edgeDefinitions;
      sGEDs.forEach(
        function(sGED) {
          var col = sGED.collection;
          if (col === eC) {
            if (JSON.stringify(sGED) !== JSON.stringify(edgeDefinition)) {
              err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code;
              err.errorMessage = col
                + " " + arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message;
              throw err;
            }
          }
        }
      );
    }
  );

  findOrCreateCollectionsByEdgeDefinitions([edgeDefinition]);

  this.__edgeDefinitions.push(edgeDefinition);
  graphCol.update(this.__name, {edgeDefinitions: this.__edgeDefinitions});
  this.__edgeCollections[edgeDefinition.collection] = db[edgeDefinition.collection];
  edgeDefinition.from.forEach(
    function(vc) {
      self[vc] = db[vc];
      //remove from __orphanCollections
      var orphanIndex = self.__orphanCollections.indexOf(vc);
      if (orphanIndex !== -1) {
        self.__orphanCollections.splice(orphanIndex, 1);
      }
      //push into __vertexCollections
      if (self.__vertexCollections[vc] === undefined) {
        self.__vertexCollections[vc] = db[vc];
      }
    }
  );
  edgeDefinition.to.forEach(
    function(vc) {
      self[vc] = db[vc];
      //remove from __orphanCollections
      var orphanIndex = self.__orphanCollections.indexOf(vc);
      if (orphanIndex !== -1) {
        self.__orphanCollections.splice(orphanIndex, 1);
      }
      //push into __vertexCollections
      if (self.__vertexCollections[vc] === undefined) {
        self.__vertexCollections[vc] = db[vc];
      }
    }
  );
  updateBindCollections(this);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function for editing edge definitions
////////////////////////////////////////////////////////////////////////////////

var changeEdgeDefinitionsForGraph = function(graph, edgeDefinition, newCollections, possibleOrphans, self) {

  var graphCollections = [];
  var graphObj = _graph(graph._key);
  var eDs = graph.edgeDefinitions;
  var gotAHit = false;
  var graphCol = getGraphCollection();

  //replace edgeDefintion
  eDs.forEach(
    function(eD, id) {
      if(eD.collection === edgeDefinition.collection) {
        gotAHit = true;
        eDs[id].from = edgeDefinition.from;
        eDs[id].to = edgeDefinition.to;
        graphCol.update(graph._key, {edgeDefinitions: eDs});
        if (graph._key === self.__name) {
          self.__edgeDefinitions[id].from = edgeDefinition.from;
          self.__edgeDefinitions[id].to = edgeDefinition.to;
        }
      } else {
        //collect all used collections
        graphCollections = _.union(graphCollections, eD.from);
        graphCollections = _.union(graphCollections, eD.to);
      }
    }
  );
  if (!gotAHit) {
    return;
  }

  //remove used collection from orphanage
  if (graph._key === self.__name) {
    newCollections.forEach(
      function(nc) {
        if (self.__vertexCollections[nc] === undefined) {
          self.__vertexCollections[nc] = db[nc];
        }
        try {
          self._removeVertexCollection(nc, false);
        } catch (ignore) { }
      }
    );
    possibleOrphans.forEach(
      function(po) {
        if (graphCollections.indexOf(po) === -1) {
          delete self.__vertexCollections[po];
          self._addVertexCollection(po);
        }
      }
    );
  } else {
    newCollections.forEach(
      function(nc) {
        try {
          graphObj._removeVertexCollection(nc, false);
        } catch (ignore) { }
      }
    );
    possibleOrphans.forEach(
      function(po) {
        if (graphCollections.indexOf(po) === -1) {
          delete graphObj.__vertexCollections[po];
          graphObj._addVertexCollection(po);
        }
      }
    );

  }

  //move unused collections to orphanage
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__editEdgeDefinition
/// @brief Modify an relation definition
///
/// `graph_module._editEdgeDefinition(edgeDefinition)`
///
/// Edits one relation definition of a graph. The edge definition used as argument will
/// replace the existing edge definition of the graph which has the same collection.
/// Vertex Collections of the replaced edge definition that are not used in the new
/// definition will transform to an orphan. Orphans that are used in this new edge
/// definition will be deleted from the list of orphans. Other graphs with the same edge
/// definition will be modified, too.
///
/// @PARAMS
///
/// @PARAM{edgeDefinition, object, required}
/// The edge definition to replace the existing edge
/// definition with the same attribute *collection*.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__editEdgeDefinition}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var original = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var modified = graph_module._relation("myEC1", ["myVC2"], ["myVC3"]);
///   var graph = graph_module._create("myGraph", [original]);
///   graph._editEdgeDefinitions(modified);
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._editEdgeDefinitions = function(edgeDefinition) {
  edgeDefinition = sortEdgeDefinition(edgeDefinition);
  var self = this;

  //check, if in graphs edge definition
  if (this.__edgeCollections[edgeDefinition.collection] === undefined) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message;
    throw err;
  }

  findOrCreateCollectionsByEdgeDefinitions([edgeDefinition]);

  //evaluate collections to add to orphanage
  var possibleOrphans = [];
  var currentEdgeDefinition;
  this.__edgeDefinitions.forEach(
    function(ed) {
      if (edgeDefinition.collection === ed.collection) {
        currentEdgeDefinition = ed;
      }
    }
  );

  var currentCollections = _.union(currentEdgeDefinition.from, currentEdgeDefinition.to);
  var newCollections = _.union(edgeDefinition.from, edgeDefinition.to);
  currentCollections.forEach(
    function(colName) {
      if (newCollections.indexOf(colName) === -1) {
        possibleOrphans.push(colName);
      }
    }
  );
  //change definition for ALL graphs
  var graphs = getGraphCollection().toArray();
  graphs.forEach(
    function(graph) {
      changeEdgeDefinitionsForGraph(graph, edgeDefinition, newCollections, possibleOrphans, self);
    }
  );
  updateBindCollections(this);
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__deleteEdgeDefinition
/// @brief Delete one relation definition
///
/// `graph_module._deleteEdgeDefinition(edgeCollectionName, dropCollection)`
///
/// Deletes a relation definition defined by the edge collection of a graph. If the
/// collections defined in the edge definition (collection, from, to) are not used
/// in another edge definition of the graph, they will be moved to the orphanage.
///
/// @PARAMS
///
/// @PARAM{edgeCollectionName, string, required}
/// Name of edge collection in the relation definition.
/// @PARAM{dropCollection, boolean, optional}
/// Define if the edge collection should be dropped. Default false.
///
/// @EXAMPLES
///
/// Remove an edge definition but keep the edge collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__deleteEdgeDefinition}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
///   var graph = graph_module._create("myGraph", [ed1, ed2]);
///   graph._deleteEdgeDefinition("myEC1");
///   db._collection("myEC1");
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Remove an edge definition and drop the edge collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__deleteEdgeDefinitionWithDrop}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
///   var graph = graph_module._create("myGraph", [ed1, ed2]);
///   graph._deleteEdgeDefinition("myEC1", true);
///   db._collection("myEC1");
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._deleteEdgeDefinition = function(edgeCollection, dropCollection) {

  //check, if in graphs edge definition
  if (this.__edgeCollections[edgeCollection] === undefined) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message;
    throw err;
  }

  var edgeDefinitions = this.__edgeDefinitions,
    self = this,
    usedVertexCollections = [],
    possibleOrphans = [],
    index;

  edgeDefinitions.forEach(
    function(edgeDefinition, idx) {
      if (edgeDefinition.collection === edgeCollection) {
        index = idx;
        possibleOrphans = edgeDefinition.from;
        possibleOrphans = _.union(possibleOrphans, edgeDefinition.to);
      } else {
        usedVertexCollections = _.union(usedVertexCollections, edgeDefinition.from);
        usedVertexCollections = _.union(usedVertexCollections, edgeDefinition.to);
      }
    }
  );
  this.__edgeDefinitions.splice(index, 1);
  possibleOrphans.forEach(
    function(po) {
      if (usedVertexCollections.indexOf(po) === -1) {
        self.__orphanCollections.push(po);
      }
    }
  );

  updateBindCollections(this);
  getGraphCollection().update(
    this.__name,
    {
      orphanCollections: this.__orphanCollections,
      edgeDefinitions: this.__edgeDefinitions
    }
  );

  if (dropCollection) {
    db._drop(edgeCollection);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__addVertexCollection
/// @brief Add a vertex collection to the graph
///
/// `graph._addVertexCollection(vertexCollectionName, createCollection)`
///
/// Adds a vertex collection to the set of orphan collections of the graph. If the
/// collection does not exist, it will be created. If it is already used by any edge
/// definition of the graph, an error will be thrown.
///
/// @PARAMS
///
/// @PARAM{vertexCollectionName, string, required}
/// Name of vertex collection.
///
/// @PARAM{createCollection, boolean, optional}
/// If true the collection will be created if it does not exist. Default: true.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__addVertexCollection}
///   var graph_module = require("org/arangodb/general-graph");
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._addVertexCollection("myVC3", true);
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._addVertexCollection = function(vertexCollectionName, createCollection) {
  //check edgeCollection
  var ec = db._collection(vertexCollectionName);
  var err;
  if (ec === null) {
    if (createCollection !== false) {
      db._create(vertexCollectionName);
    } else {
      err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
      err.errorMessage = vertexCollectionName + arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message;
      throw err;
    }
  } else if (ec.type() !== 2) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX.message;
    throw err;
  }
  if (this.__vertexCollections[vertexCollectionName] !== undefined) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF.message;
    throw err;
  }
  if (_.contains(this.__orphanCollections, vertexCollectionName)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.message;
    throw err;
  }
  this.__orphanCollections.push(vertexCollectionName);
  updateBindCollections(this);
  getGraphCollection().update(this.__name, {orphanCollections: this.__orphanCollections});

};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__orphanCollections
/// @brief Get all orphan collections
///
/// `graph._orphanCollections()`
///
/// Returns all vertex collections of the graph that are not used in any edge definition.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__orphanCollections}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._addVertexCollection("myVC3", true);
///   graph._orphanCollections();
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._orphanCollections = function() {
  return this.__orphanCollections;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__removeVertexCollection
/// @brief Remove a vertex collection from the graph
///
/// `graph._removeVertexCollection(vertexCollectionName, dropCollection)`
///
/// Removes a vertex collection from the graph.
/// Only collections not used in any relation definition can be removed.
/// Optionally the collection can be deleted, if it is not used in any other graph.
///
/// @PARAMS
///
/// @PARAM{vertexCollectionName, string, required}
/// Name of vertex collection.
///
/// @PARAM{dropCollection, boolean, optional}
/// If true the collection will be dropped if it is
/// not used in any other graph. Default: false.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__removeVertexCollections}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._addVertexCollection("myVC3", true);
///   graph._addVertexCollection("myVC4", true);
///   graph._orphanCollections();
///   graph._removeVertexCollection("myVC3");
///   graph._orphanCollections();
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._removeVertexCollection = function(vertexCollectionName, dropCollection) {
  var err;
  if (db._collection(vertexCollectionName) === null) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message;
    throw err;
  }
  var index = this.__orphanCollections.indexOf(vertexCollectionName);
  if (index === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION.message;
    throw err;
  }
  this.__orphanCollections.splice(index, 1);
  delete this[vertexCollectionName];
  getGraphCollection().update(this.__name, {orphanCollections: this.__orphanCollections});

  if (dropCollection === true) {
    var graphs = getGraphCollection().toArray();
    if (checkIfMayBeDropped(vertexCollectionName, null, graphs)) {
      db._drop(vertexCollectionName);
    }
  }
  updateBindCollections(this);
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_connectingEdges
/// @brief Get all connecting edges between 2 groups of vertices defined by the examples
///
/// `graph._connectingEdges(vertexExample, vertexExample2, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for vertexExample.
///
/// @PARAMS
///
/// @PARAM{vertexExample1, object, optional}
/// See [Definition of examples](#definition_of_examples)
/// @PARAM{vertexExample2, object, optional}
/// See [Definition of examples](#definition_of_examples)
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *edgeExamples*: Filter the edges, see [Definition of examples](#definition_of_examples)
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *vertex1CollectionRestriction* : One or a list of vertex-collection names that should be
///       considered on the intermediate vertex steps.
///   * *vertex2CollectionRestriction* : One or a list of vertex-collection names that should be
///       considered on the intermediate vertex steps.
///
/// @EXAMPLES
///
/// A route planner example, all connecting edges between capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleConnectingEdges1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._getConnectingEdges({isCapital : true}, {isCapital : true});
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getConnectingEdges = function(vertexExample1, vertexExample2, options) {

  if (!options) {
    options = {};
  }

  var opts = {
  };

  if (options.vertex1CollectionRestriction) {
    opts.startVertexCollectionRestriction = options.vertex1CollectionRestriction;
  }

  if (options.vertex2CollectionRestriction) {
    opts.endVertexCollectionRestriction = options.vertex2CollectionRestriction;
  }

  if (options.edgeCollectionRestriction) {
    opts.edgeCollectionRestriction = options.edgeCollectionRestriction;
  }

  if (options.edgeExamples) {
    opts.edgeExamples = options.edgeExamples;
  }

  if (vertexExample2) {
    opts.neighborExamples = vertexExample2;
  }

  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "vertexExample": vertexExample1,
    "options": opts
  };
  var result = db._query(EDGES, bindVars).toArray();
  return result[0];
};




////////////////////////////////////////////////////////////////////////////////
/// @brief print basic information for the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._PRINT = function(context) {
  var name = this.__name;
  var edgeDefs = printEdgeDefinitions(this.__edgeDefinitions);
  context.output += "[ Graph ";
  context.output += name;
  context.output += " EdgeDefinitions: ";
  internal.printRecursive(edgeDefs, context);
  context.output += " VertexCollections: ";
  internal.printRecursive(this.__orphanCollections, context);
  context.output += " ]";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

/// Deprecated function (announced 2.3)
exports._undirectedRelation = _undirectedRelation;
/// Deprecated function (announced 2.3)
exports._directedRelation = function () {
  return _relation.apply(this, arguments);
} ;
exports._relation = _relation;
exports._graph = _graph;
exports._edgeDefinitions = _edgeDefinitions;
exports._extendEdgeDefinitions = _extendEdgeDefinitions;
exports._create = _create;
exports._drop = _drop;
exports._exists = _exists;
exports._list = _list;
exports._listObjects = _listObjects;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:


////////////////////////////////////////////////////////////////////////////////
/// some more documentation
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_create_graph_example1
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph_create_graph_example1}
///   var graph_module = require("org/arangodb/general-graph");
///   var edgeDefinitions = graph_module._edgeDefinitions();
///   graph_module._extendEdgeDefinitions(edgeDefinitions, graph_module._relation("friend_of", "Customer", "Customer"));
/// | graph_module._extendEdgeDefinitions(
/// | edgeDefinitions, graph_module._relation(
///   "has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]));
///   graph_module._create("myStore", edgeDefinitions);
/// ~ graph_module._drop("myStore");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_create_graph_example2
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph_create_graph_example2}
///   var graph_module = require("org/arangodb/general-graph");
/// |  var edgeDefinitions = graph_module._edgeDefinitions(
/// |  graph_module._relation("friend_of", ["Customer"]), graph_module._relation(
///    "has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]));
///   graph_module._create("myStore", edgeDefinitions);
/// ~ graph_module._drop("myStore");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
});
