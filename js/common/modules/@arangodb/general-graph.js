/* jshint strict: false */
/* global ArangoClusterComm */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Graph functionality
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2014 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Florian Bartels, Michael Hackstein, Guido Schwab
// / @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb'),
  internal = require('internal'),
  ArangoCollection = arangodb.ArangoCollection,
  ArangoError = arangodb.ArangoError,
  db = arangodb.db,
  errors = arangodb.errors,
  _ = require('lodash');

// //////////////////////////////////////////////////////////////////////////////
  // / @brief Compatibility functions for 2.8
  // /        This function registeres user-defined functions that follow the
  // /        same API as the former GRAPH_* functions did.
  // /        Most of these AQL functions can be simply replaced by calls to these.
  // //////////////////////////////////////////////////////////////////////////////

var registerCompatibilityFunctions = function () {
  var aqlfunctions = require('@arangodb/aql/functions');
  aqlfunctions.register('arangodb::GRAPH_EDGES', function (graphName, vertexExample, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._edges(vertexExample, options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_VERTICES', function (graphName, vertexExample, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._vertices(vertexExample, options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_NEIGHBORS', function (graphName, vertexExample, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._neighbors(vertexExample, options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_COMMON_NEIGHBORS', function (graphName, vertex1Example, vertex2Example, options1, options2) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._commonNeighbors(vertex1Example, vertex2Example, options1, options2);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_COMMON_PROPERTIES', function (graphName, vertex1Example, vertex2Example, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._commonProperties(vertex1Example, vertex2Example, options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_PATHS', function (graphName, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._paths(options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_SHORTEST_PATH', function (graphName, startVertexExample, edgeVertexExample, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._shortestPath(startVertexExample, edgeVertexExample, options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_DISTANCE_TO', function (graphName, startVertexExample, edgeVertexExample, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._distanceTo(startVertexExample, edgeVertexExample, options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_ABSOLUTE_ECCENTRICITY', function (graphName, vertexExample, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._absoluteEccentricity(vertexExample, options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_ECCENTRICITY', function (graphName, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._eccentricity(options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_ABSOLUTE_CLOSENESS', function (graphName, vertexExample, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._farness(vertexExample, options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_CLOSENESS', function (graphName, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._closeness(options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_ABSOLUTE_BETWEENNESS', function (graphName, vertexExample, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._absoluteBetweenness(vertexExample, options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_BETWEENNESS', function (graphName, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._betweenness(options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_RADIUS', function (graphName, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._radius(options);
  }, false);
  aqlfunctions.register('arangodb::GRAPH_DIAMETER', function (graphName, options) {
    var gm = require('@arangodb/general-graph');
    var g = gm._graph(graphName);
    return g._diameter(options);
  }, false);
};

var fixWeight = function (options) {
  if (!options.hasOwnProperty('weightAttribute') && options.hasOwnProperty('weight')) {
    options.weightAttribute = options.weight;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief transform a string into an array.
// //////////////////////////////////////////////////////////////////////////////

var stringToArray = function (x) {
  if (typeof x === 'string') {
    return [x];
  }
  return x.slice();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks if a parameter is not defined, an empty string or an empty
//  array
// //////////////////////////////////////////////////////////////////////////////

var isValidCollectionsParameter = function (x) {
  if (!x) {
    return false;
  }
  if (Array.isArray(x) && x.length === 0) {
    return false;
  }
  if (typeof x !== 'string' && !Array.isArray(x)) {
    return false;
  }
  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief find or create a collection by name
// //////////////////////////////////////////////////////////////////////////////

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
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION.code;
    err.errorMessage = name + arangodb.errors.ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION.message;
    throw err;
  } else if (type === ArangoCollection.TYPE_EDGE && col.type() !== type) {
    var err2 = new ArangoError();
    err2.errorNum = arangodb.errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code;
    err2.errorMessage = name + ' cannot be used as relation. It is not an edge collection';
    throw err2;
  }
  return res;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief find or create a collection by name
// //////////////////////////////////////////////////////////////////////////////

var findOrCreateCollectionsByEdgeDefinitions = function (edgeDefinitions, noCreate) {
  var vertexCollections = {},
    edgeCollections = {};
  edgeDefinitions.forEach(function (e) {
    if (!e.hasOwnProperty('collection') ||
      !e.hasOwnProperty('from') ||
      !e.hasOwnProperty('to') ||
      !Array.isArray(e.from) ||
      !Array.isArray(e.to)) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal function to get graphs collection
// //////////////////////////////////////////////////////////////////////////////

var getGraphCollection = function () {
  var gCol = db._graphs;
  if (gCol === null || gCol === undefined) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NO_GRAPH_COLLECTION.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NO_GRAPH_COLLECTION.message;
    throw err;
  }
  return gCol;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal function to print edge definitions in _PRINT
// //////////////////////////////////////////////////////////////////////////////

var printEdgeDefinitions = function (defs) {
  return _.map(defs, function (d) {
    var out = d.collection;
    out += ': [';
    out += d.from.join(', ');
    out += '] -> [';
    out += d.to.join(', ');
    out += ']';
    return out;
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal function to wrap arango collections for overwrite
// //////////////////////////////////////////////////////////////////////////////

var wrapCollection = function (col) {
  var wrapper = {};
  _.each(_.functionsIn(col), function (func) {
    wrapper[func] = function () {
      return col[func].apply(col, arguments);
    };
  });
  return wrapper;
};

// Returns either `collection` or UNION(`all collections`)
// Does not contain filters or iterator variable
// Can be used as for x IN ${startInAllCollections()} return x
var startInAllCollections = function (collections) {
  if (collections.length === 1) {
    return `${collections[0]}`;
  }
  return `UNION(${collections.map(c => `(FOR x IN ${c} RETURN x)`).join(", ")})`;
};

var buildEdgeCollectionRestriction = function (collections, bindVars, graph) {
  if (typeof collections === "string") {
    collections = [collections];
  }
  if (!Array.isArray(collections) || collections.length === 0) {
    bindVars.graphName = graph.__name;
    return "GRAPH @graphName";
  }
  return collections.map(collection => '`' + collection + '`').join(',');
};

var buildVertexCollectionRestriction = function (collections, varname) {
  if (!Array.isArray(collections)) {
    collections = [ collections ];
  }
  var filter = `FILTER (
    ${collections.map(e => {
      return `IS_SAME_COLLECTION(${JSON.stringify(e)},${varname})`;
    }).join(" OR ")}
  )`;
  return `${filter}`;
};

var buildFilter = function (examples, bindVars, varname) {
  var varcount = 0;
  var foundAllMatch = false;
  if (!Array.isArray(examples)) {
    examples = [ examples ];
  }
  var filter = `FILTER (
    ${examples.map(e => {
      if (typeof e === "object") {
        var keys = Object.keys(e);
        if (keys.length === 0) {
          foundAllMatch = true;
          return "";
        }
        return keys.map(key => {
          bindVars[varname + "ExVar" + varcount] = key;
          bindVars[varname + "ExVal" + varcount] = e[key];
          return `${varname}[@${varname}ExVar${varcount}] == @${varname}ExVal${varcount++}`;
        }).join(" AND ");
      } else if(typeof e === "string") {
        bindVars[varname + "ExVar" + varcount] = e;
        return `${varname}._id == @${varname}ExVar${varcount++}`;
      } else {
        foundAllMatch = true;
        return "";
      }
    }).join(") OR (")}
  )`;
  if (foundAllMatch) {
    for (var i = 0; i < varcount; ++i) {
      delete bindVars[varname + 'ExVar' + varcount];
      delete bindVars[varname + 'ExVal' + varcount];
    }
    return ``;
  }
  return `${filter}`;
};

// Returns WITH v1, ... vn if we do not use the graph name.
var generateWithStatement = function (graph, options) {
  if (!options.hasOwnProperty("edgeCollectionRestriction")
    || !Array.isArray(options.edgeCollectionRestriction)
    || options.edgeCollectionRestriction.length === 0) {
    return "";
  }
  return "WITH " + Object.keys(graph.__vertexCollections).join(", ");
};

// Returns FOR <varname> IN (...)
// So start contains every object in the graph
// matching the example(s)
var transformExampleToAQL = function (examples, collections, bindVars, varname) {
  var varcount = 0;
  var foundAllMatch = false;
  if (!Array.isArray(examples)) {
    examples = [ examples ];
  }
  var filter = `FILTER (
    ${examples.map(e => {
      if (typeof e === "object") {
        var keys = Object.keys(e);
        if (keys.length === 0) {
          foundAllMatch = true;
          return "";
        }
        return keys.map(key => {
          bindVars[varname + "ExVar" + varcount] = key;
          bindVars[varname + "ExVal" + varcount] = e[key];
          return `${varname}[@${varname}ExVar${varcount}] == @${varname}ExVal${varcount++}`;
        }).join(" AND ");
      } else {
        bindVars[varname + "ExVar" + varcount] = e;
        return `${varname}._id == @${varname}ExVar${varcount++}`;
      }
    }).join(") OR (")}
  )`;
  if (foundAllMatch) {
    for (var i = 0; i < varcount; ++i) {
      delete bindVars[varname + 'ExVar' + varcount];
      delete bindVars[varname + 'ExVal' + varcount];
    }
    return `FOR ${varname} IN ${startInAllCollections(collections)} `;
  }
  var query = `FOR ${varname} IN `;
  if (collections.length === 1) {
    query += `${collections[0]} ${filter}`;
  } else {
    query += `UNION (${collections.map(c => `(FOR ${varname} IN ${c} ${filter} RETURN ${varname})`).join(", ")}) `;
  }
  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_relation
// //////////////////////////////////////////////////////////////////////////////

var _relation = function (
  relationName, fromVertexCollections, toVertexCollections) {
  var err;
  if (arguments.length < 3) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.message + '3';
    throw err;
  }

  if (typeof relationName !== 'string' || relationName === '') {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message + ' arg1 must be non empty string';
    throw err;
  }

  if (!isValidCollectionsParameter(fromVertexCollections)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message
      + ' arg2 must be non empty string or array';
    throw err;
  }

  if (!isValidCollectionsParameter(toVertexCollections)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message
      + ' arg3 must be non empty string or array';
    throw err;
  }

  return {
    collection: relationName,
    from: stringToArray(fromVertexCollections),
    to: stringToArray(toVertexCollections)
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_list
// //////////////////////////////////////////////////////////////////////////////

var _list = function () {
  var gdb = getGraphCollection();
  return _.map(gdb.toArray(), '_key');
};

var _listObjects = function () {
  return getGraphCollection().toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_edge_definitions
// //////////////////////////////////////////////////////////////////////////////

var _edgeDefinitions = function () {
  var res = [], args = arguments;
  Object.keys(args).forEach(function (x) {
    res.push(args[x]);
  });

  return res;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_extend_edge_definitions
// //////////////////////////////////////////////////////////////////////////////

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
// //////////////////////////////////////////////////////////////////////////////
// / internal helper to sort a graph's edge definitions
// //////////////////////////////////////////////////////////////////////////////
var sortEdgeDefinition = function (edgeDefinition) {
  edgeDefinition.from = edgeDefinition.from.sort();
  edgeDefinition.to = edgeDefinition.to.sort();
  return edgeDefinition;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief create a new graph
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_how_to_create
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_create
// //////////////////////////////////////////////////////////////////////////////

var _create = function (graphName, edgeDefinitions, orphanCollections, options) {
  if (!Array.isArray(orphanCollections)) {
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
  // check, if a collection is already used in a different edgeDefinition
  var tmpCollections = [];
  var tmpEdgeDefinitions = {};
  edgeDefinitions.forEach(
    function (edgeDefinition) {
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
    function (singleGraph) {
      var sGEDs = singleGraph.edgeDefinitions;
      sGEDs.forEach(
        function (sGED) {
          var col = sGED.collection;
          if (tmpCollections.indexOf(col) !== -1) {
            if (JSON.stringify(sGED) !== JSON.stringify(tmpEdgeDefinitions[col])) {
              err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code;
              err.errorMessage = col + ' '
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
    function (oC) {
      findOrCreateCollectionByName(oC, ArangoCollection.TYPE_DOCUMENT);
    }
  );

  edgeDefinitions.forEach(
    function (eD, index) {
      var tmp = sortEdgeDefinition(eD);
      edgeDefinitions[index] = tmp;
    }
  );
  orphanCollections = orphanCollections.sort();

  var data = gdb.save({
    'orphanCollections': orphanCollections,
    'edgeDefinitions': edgeDefinitions,
    '_key': graphName
  }, options);

  result = new Graph(graphName, edgeDefinitions, collections[0], collections[1],
    orphanCollections, data._rev , data._id);
  return result;
};

var createHiddenProperty = function (obj, name, value) {
  Object.defineProperty(obj, name, {
    enumerable: false,
    writable: true
  });
  obj[name] = value;
};
// //////////////////////////////////////////////////////////////////////////////
// / @brief helper for updating binded collections
// //////////////////////////////////////////////////////////////////////////////
var removeEdge = function (graphs, edgeCollection, edgeId, self) {
  self.__idsToRemove[edgeId] = 1;
  graphs.forEach(
    function (graph) {
      var edgeDefinitions = graph.edgeDefinitions;
      if (graph.edgeDefinitions) {
        edgeDefinitions.forEach(
          function (edgeDefinition) {
            var from = edgeDefinition.from;
            var to = edgeDefinition.to;
            var collection = edgeDefinition.collection;
            // if collection of edge to be deleted is in from or to
            if (from.indexOf(edgeCollection) !== -1 || to.indexOf(edgeCollection) !== -1) {
              // search all edges of the graph
              var edges = db._collection(collection).edges(edgeId);
              edges.forEach(function (edge) {
                // if from is
                if (!self.__idsToRemove.hasOwnProperty(edge._id)) {
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

var bindEdgeCollections = function (self, edgeCollections) {
  _.each(edgeCollections, function (key) {
    var obj = db._collection(key);
    var wrap = wrapCollection(obj);
    // save
    var old_save = wrap.insert;
    wrap.save = wrap.insert = function (from, to, data) {
      if (typeof from === 'object' && to === undefined) {
        data = from;
        from = data._from;
        to = data._to;
      } else if (typeof from === 'string' && typeof to === 'string' && typeof data === 'object') {
        data._from = from;
        data._to = to;
      }

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

      // check, if edge is allowed
      self.__edgeDefinitions.forEach(
        function (edgeDefinition) {
          if (edgeDefinition.collection === key) {
            var fromCollection = from.split('/')[0];
            var toCollection = to.split('/')[0];
            if (!_.includes(edgeDefinition.from, fromCollection)
              || !_.includes(edgeDefinition.to, toCollection)) {
              var err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_EDGE.code;
              err.errorMessage =
                arangodb.errors.ERROR_GRAPH_INVALID_EDGE.message +
                ' between ' + from + ' and ' + to + '. Doesn\'t conform to any edge definition';
              throw err;
            }
          }
        }
      );
      return old_save(data);
    };

    // remove
    wrap.remove = function (edgeId, options) {
      // if _key make _id (only on 1st call)
      if (edgeId.indexOf('/') === -1) {
        edgeId = key + '/' + edgeId;
      }
      var graphs = getGraphCollection().toArray();
      var edgeCollection = edgeId.split('/')[0];
      self.__collectionsToLock[edgeCollection] = 1;
      removeEdge(graphs, edgeCollection, edgeId, self);

      try {
        db._executeTransaction({
          collections: {
            write: Object.keys(self.__collectionsToLock)
          },
          embed: true,
          action: function (params) {
            var db = require('internal').db;
            params.ids.forEach(
              function (edgeId) {
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

var bindVertexCollections = function (self, vertexCollections) {
  _.each(vertexCollections, function (key) {
    var obj = db._collection(key);
    var wrap = wrapCollection(obj);
    wrap.remove = function (vertexId, options) {
      // delete all edges using the vertex in all graphs
      var graphs = getGraphCollection().toArray();
      var vertexCollectionName = key;
      if (vertexId.indexOf('/') === -1) {
        vertexId = key + '/' + vertexId;
      }
      self.__collectionsToLock[vertexCollectionName] = 1;
      graphs.forEach(
        function (graph) {
          var edgeDefinitions = graph.edgeDefinitions;
          if (graph.edgeDefinitions) {
            edgeDefinitions.forEach(
              function (edgeDefinition) {
                var from = edgeDefinition.from;
                var to = edgeDefinition.to;
                var collection = edgeDefinition.collection;
                if (from.indexOf(vertexCollectionName) !== -1
                  || to.indexOf(vertexCollectionName) !== -1
                ) {
                  var edges = db._collection(collection).edges(vertexId);
                  if (edges.length > 0) {
                    self.__collectionsToLock[collection] = 1;
                    edges.forEach(function (edge) {
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
            var db = require('internal').db;
            params.ids.forEach(
              function (edgeId) {
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
var updateBindCollections = function (graph) {
  // remove all binded collections
  Object.keys(graph).forEach(
    function (key) {
      if (key.substring(0, 1) !== '_') {
        delete graph[key];
      }
    }
  );
  graph.__edgeDefinitions.forEach(
    function (edgeDef) {
      bindEdgeCollections(graph, [edgeDef.collection]);
      bindVertexCollections(graph, edgeDef.from);
      bindVertexCollections(graph, edgeDef.to);
    }
  );
  bindVertexCollections(graph, graph.__orphanCollections);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_vertex_collection_save
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_vertex_collection_replace
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_vertex_collection_update
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_vertex_collection_remove
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_edge_collection_save
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_edge_collection_replace
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_edge_collection_update
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_edge_collection_remove
// //////////////////////////////////////////////////////////////////////////////
var Graph = function (graphName, edgeDefinitions, vertexCollections, edgeCollections,
  orphanCollections, revision, id) {
  edgeDefinitions.forEach(
    function (eD, index) {
      var tmp = sortEdgeDefinition(eD);
      edgeDefinitions[index] = tmp;
    }
  );

  if (!orphanCollections) {
    orphanCollections = [];
  }

  // we can call the "fast" version of some edge functions if we are
  // running server-side and are not a coordinator
  var useBuiltIn = (typeof ArangoClusterComm === 'object');
  if (useBuiltIn && require('@arangodb/cluster').isCoordinator()) {
    useBuiltIn = false;
  }

  var self = this;
  // Create Hidden Properties
  createHiddenProperty(this, '__useBuiltIn', useBuiltIn);
  createHiddenProperty(this, '__name', graphName);
  createHiddenProperty(this, '__vertexCollections', vertexCollections);
  createHiddenProperty(this, '__edgeCollections', edgeCollections);
  createHiddenProperty(this, '__edgeDefinitions', edgeDefinitions);
  createHiddenProperty(this, '__idsToRemove', {});
  createHiddenProperty(this, '__collectionsToLock', {});
  createHiddenProperty(this, '__id', id);
  createHiddenProperty(this, '__rev', revision);
  createHiddenProperty(this, '__orphanCollections', orphanCollections);
  updateBindCollections(self);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_graph
// //////////////////////////////////////////////////////////////////////////////

var _graph = function (graphName) {
  var gdb = getGraphCollection(),
    g, collections, orphanCollections;

  try {
    g = gdb.document(graphName);
  } catch (e) {
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
    g._rev, g._id);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief check if a graph exists.
// //////////////////////////////////////////////////////////////////////////////

var _exists = function (graphId) {
  var gCol = getGraphCollection();
  return gCol.exists(graphId);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief rename a collection inside the _graphs collections
// //////////////////////////////////////////////////////////////////////////////

var _renameCollection = function (oldName, newName) {
  db._executeTransaction({
    collections: {
      write: '_graphs'
    },
    action: function (params) {
      var gdb = getGraphCollection();
      if (!gdb) {
        return;
      }
      gdb.toArray().forEach(function (doc) {
        var c = Object.assign({}, doc), i, j, changed = false;
        if (c.edgeDefinitions) {
          for (i = 0; i < c.edgeDefinitions.length; ++i) {
            var def = c.edgeDefinitions[i];
            if (def.collection === params.oldName) {
              c.edgeDefinitions[i].collection = params.newName;
              changed = true;
            }
            for (j = 0; j < def.from.length; ++j) {
              if (def.from[j] === params.oldName) {
                c.edgeDefinitions[i].from[j] = params.newName;
                changed = true;
              }
            }
            for (j = 0; j < def.to.length; ++j) {
              if (def.to[j] === params.oldName) {
                c.edgeDefinitions[i].to[j] = params.newName;
                changed = true;
              }
            }
          }
        }
        for (i = 0; i < c.orphanCollections.length; ++i) {
          if (c.orphanCollections[i] === params.oldName) {
            c.orphanCollections[i] = params.newName;
            changed = true;
          }
        }

        if (changed) {
          gdb.update(doc._key, c);
        }
      });
    },
    params: {
      oldName: oldName,
      newName: newName
    }
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Helper for dropping collections of a graph.
// //////////////////////////////////////////////////////////////////////////////

var checkIfMayBeDropped = function (colName, graphName, graphs) {
  var result = true;
  graphs.forEach(
    function (graph) {
      if (graph._key === graphName) {
        return;
      }
      var edgeDefinitions = graph.edgeDefinitions;
      if (edgeDefinitions) {
        edgeDefinitions.forEach(
          function (edgeDefinition) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_drop
// //////////////////////////////////////////////////////////////////////////////

var _drop = function (graphId, dropCollections) {
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
      function (edgeDefinition) {
        var from = edgeDefinition.from;
        var to = edgeDefinition.to;
        var collection = edgeDefinition.collection;
        graphs = getGraphCollection().toArray();
        if (checkIfMayBeDropped(collection, graph._key, graphs)) {
          db._drop(collection);
        }
        from.forEach(
          function (col) {
            if (checkIfMayBeDropped(col, graph._key, graphs)) {
              db._drop(col);
            }
          }
        );
        to.forEach(
          function (col) {
            if (checkIfMayBeDropped(col, graph._key, graphs)) {
              db._drop(col);
            }
          }
        );
      }
    );
    // drop orphans
    graphs = getGraphCollection().toArray();
    if (!graph.orphanCollections) {
      graph.orphanCollections = [];
    }
    graph.orphanCollections.forEach(
      function (oC) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return all edge collections of the graph.
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._edgeCollections = function () {
  return _.values(this.__edgeCollections);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return all vertex collections of the graph.
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._vertexCollections = function (excludeOrphans) {
  if (excludeOrphans) {
    return this.__vertexCollections;
  }
  var orphans = [];
  _.each(this.__orphanCollections, function (o) {
    orphans.push(db[o]);
  });
  return _.union(_.values(this.__vertexCollections), orphans);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief _EDGES(vertexId).
// //////////////////////////////////////////////////////////////////////////////

// might be needed from AQL itself
Graph.prototype._EDGES = function (vertexId) {
  var err;
  if (vertexId.indexOf('/') === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ': ' + vertexId;
    throw err;
  }

  var result = [], c;
  for (c in this.__edgeCollections) {
    if (this.__edgeCollections.hasOwnProperty(c)) {
      if (this.__useBuiltIn) {
        result = result.concat(this.__edgeCollections[c].EDGES(vertexId));
      } else {
        result = result.concat(this.__edgeCollections[c].edges(vertexId));
      }
    }
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief INEDGES(vertexId).
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._INEDGES = function (vertexId) {
  var err;
  if (vertexId.indexOf('/') === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ': ' + vertexId;
    throw err;
  }

  var result = [], c;
  for (c in this.__edgeCollections) {
    if (this.__edgeCollections.hasOwnProperty(c)) {
      if (this.__useBuiltIn) {
        result = result.concat(this.__edgeCollections[c].INEDGES(vertexId));
      } else {
        result = result.concat(this.__edgeCollections[c].inEdges(vertexId));
      }
    }
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief outEdges(vertexId).
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._OUTEDGES = function (vertexId) {
  var err;
  if (vertexId.indexOf('/') === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ': ' + vertexId;
    throw err;
  }

  var result = [], c;
  for (c in this.__edgeCollections) {
    if (this.__edgeCollections.hasOwnProperty(c)) {
      if (this.__useBuiltIn) {
        result = result.concat(this.__edgeCollections[c].OUTEDGES(vertexId));
      } else {
        result = result.concat(this.__edgeCollections[c].outEdges(vertexId));
      }
    }
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_edges
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._edges = function (vertexExample, options) {
  var bindVars = {};
  options = options || {};
  var query = `
    ${transformExampleToAQL(vertexExample, Object.keys(this.__vertexCollections), bindVars, "start")}
    FOR v, e IN ${options.minDepth || 1}..${options.maxDepth || 1} ${options.direction || "ANY"} start 
    ${buildEdgeCollectionRestriction(options.edgeCollectionRestriction, bindVars, this)}
    ${buildFilter(options.edgeExamples, bindVars, "e")} 
    RETURN DISTINCT ${options.includeData === true ? "e" : "e._id"}`;
  return db._query(query, bindVars).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_vertices
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._vertices = function (vertexExample, options) {
  options = options || {};
  if (options.vertexCollectionRestriction) {
    if (!Array.isArray(options.vertexCollectionRestriction)) {
      options.vertexCollectionRestriction = [ options.vertexCollectionRestriction ];
    }
  }
  var bindVars = {};
  var query = `${transformExampleToAQL({}, Array.isArray(options.vertexCollectionRestriction) && options.vertexCollectionRestriction.length > 0 ? options.vertexCollectionRestriction :  Object.keys(this.__vertexCollections), bindVars, "start")} 
  ${buildFilter(vertexExample, bindVars, "start")} 
  RETURN DISTINCT start`;
  return db._query(query, bindVars).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_fromVertex
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._fromVertex = function (edgeId) {
  if (typeof edgeId !== 'string' ||
    edgeId.indexOf('/') === -1) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
    throw err;
  }
  var edgeCollection = this._getEdgeCollectionByName(edgeId.split('/')[0]);
  var document = edgeCollection.document(edgeId);
  if (document) {
    var vertexId = document._from;
    var vertexCollection = this._getVertexCollectionByName(vertexId.split('/')[0]);
    return vertexCollection.document(vertexId);
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_toVertex
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._toVertex = function (edgeId) {
  if (typeof edgeId !== 'string' ||
    edgeId.indexOf('/') === -1) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
    throw err;
  }
  var edgeCollection = this._getEdgeCollectionByName(edgeId.split('/')[0]);
  var document = edgeCollection.document(edgeId);
  if (document) {
    var vertexId = document._to;
    var vertexCollection = this._getVertexCollectionByName(vertexId.split('/')[0]);
    return vertexCollection.document(vertexId);
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief get edge collection by name.
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._getEdgeCollectionByName = function (name) {
  if (this.__edgeCollections[name]) {
    return this.__edgeCollections[name];
  }
  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.code;
  err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.message + ': ' + name;
  throw err;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief get vertex collection by name.
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._getVertexCollectionByName = function (name) {
  if (this.__vertexCollections[name]) {
    return this.__vertexCollections[name];
  }
  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
  err.errorMessage = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message + ': ' + name;
  throw err;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_neighbors
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._neighbors = function (vertexExample, options) {
  options = options || {};
  if (options.vertexCollectionRestriction) {
    if (!Array.isArray(options.vertexCollectionRestriction)) {
      options.vertexCollectionRestriction = [ options.vertexCollectionRestriction ];
    }
  }

  var bindVars = {};
  var query = `
    ${generateWithStatement(this, options)}
    ${transformExampleToAQL(vertexExample, Object.keys(this.__vertexCollections), bindVars, "start")}
    FOR v, e IN ${options.minDepth || 1}..${options.maxDepth || 1} ${options.direction || "ANY"} start
    ${buildEdgeCollectionRestriction(options.edgeCollectionRestriction, bindVars, this)}
    OPTIONS {bfs: true}
    ${buildFilter(options.neighborExamples, bindVars, "v")}
    ${buildFilter(options.edgeExamples, bindVars, "e")}
    ${Array.isArray(options.vertexCollectionRestriction) && options.vertexCollectionRestriction.length > 0 ? buildVertexCollectionRestriction(options.vertexCollectionRestriction,"v") : ""}
    RETURN DISTINCT ${options.includeData === true ? "v" : "v._id"}`;
  return db._query(query, bindVars).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_common_neighbors
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._commonNeighbors = function (vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {
  var bindVars = {};
  optionsVertex1 = optionsVertex1 || {};
  optionsVertex2 = optionsVertex2 || {};
  if (optionsVertex1.vertexCollectionRestriction) {
    if (!Array.isArray(optionsVertex1.vertexCollectionRestriction)) {
      optionsVertex1.vertexCollectionRestriction = [ optionsVertex1.vertexCollectionRestriction ];
    }
  }
  if (optionsVertex2.vertexCollectionRestriction) {
    if (!Array.isArray(optionsVertex2.vertexCollectionRestriction)) {
      optionsVertex2.vertexCollectionRestriction = [ optionsVertex2.vertexCollectionRestriction ];
    }
  }
  var query = `
    ${generateWithStatement(this, optionsVertex1.hasOwnProperty("edgeCollectionRestriction") ? optionsVertex1 : optionsVertex2 )}
    ${transformExampleToAQL(vertex1Example, Object.keys(this.__vertexCollections), bindVars, "left")}
      LET leftNeighbors = (FOR v IN ${optionsVertex1.minDepth || 1}..${optionsVertex1.maxDepth || 1} ${optionsVertex1.direction || "ANY"} left
        ${buildEdgeCollectionRestriction(optionsVertex1.edgeCollectionRestriction, bindVars, this)}
        OPTIONS {bfs: true, uniqueVertices: "global"} 
        ${Array.isArray(optionsVertex1.vertexCollectionRestriction) && optionsVertex1.vertexCollectionRestriction.length > 0 ? buildVertexCollectionRestriction(optionsVertex1.vertexCollectionRestriction,"v") : ""} 
        RETURN v)
      ${transformExampleToAQL(vertex2Example, Object.keys(this.__vertexCollections), bindVars, "right")}
        FILTER right != left
        LET rightNeighbors = (FOR v IN ${optionsVertex2.minDepth || 1}..${optionsVertex2.maxDepth || 1} ${optionsVertex2.direction || "ANY"} right
        ${buildEdgeCollectionRestriction(optionsVertex2.edgeCollectionRestriction, bindVars, this)}
        OPTIONS {bfs: true, uniqueVertices: "global"} 
        ${Array.isArray(optionsVertex2.vertexCollectionRestriction) && optionsVertex2.vertexCollectionRestriction.length > 0 ? buildVertexCollectionRestriction(optionsVertex2.vertexCollectionRestriction,"v") : ""} 
        RETURN v)
        LET neighbors = INTERSECTION(leftNeighbors, rightNeighbors)
        FILTER LENGTH(neighbors) > 0 `;
  if (optionsVertex1.includeData === true || optionsVertex2.includeData === true) {
    query += `RETURN {left : left, right: right, neighbors: neighbors}`;
  } else {
    query += `RETURN {left : left._id, right: right._id, neighbors: neighbors[*]._id}`;
  }
  return db._query(query, bindVars).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_count_common_neighbors
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._countCommonNeighbors = function (vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {
  var result = this._commonNeighbors(vertex1Example, vertex2Example, optionsVertex1, optionsVertex2),
    tmp = {}, tmp2 = {}, returnHash = [];
  result.forEach(function (r) {
    if (!tmp[r.left]) {
      tmp[r.left] = [];
    }
    tmp2 = {};
    tmp2[r.right] = r.neighbors.length;
    tmp[r.left].push(tmp2);
  });
  Object.keys(tmp).forEach(function (w) {
    tmp2 = {};
    tmp2[w] = tmp[w];
    returnHash.push(tmp2);
  });
  return returnHash;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_common_properties
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._commonProperties = function (vertex1Example, vertex2Example, options) {
  options = options || {};
  if (options.hasOwnProperty('ignoreProperties')) {
    if (!Array.isArray(options.ignoreProperties)) {
      options.ignoreProperties = [options.ignoreProperties];
    }
  }
  var bindVars = {};
  var query = `
    ${generateWithStatement(this, options)}
    ${transformExampleToAQL(vertex1Example, Object.keys(this.__vertexCollections), bindVars, "left")}
      SORT left._id
      LET toZip = (
        ${transformExampleToAQL(vertex2Example, Object.keys(this.__vertexCollections), bindVars, "right")}
        FILTER right != left
        LET shared = (FOR a IN ATTRIBUTES(left) FILTER a != "_rev" FILTER
          (${options.hasOwnProperty("ignoreProperties") ? `a NOT IN ${JSON.stringify(options.ignoreProperties)} AND` : ""} left[a] == right[a])
          OR a == '_id' RETURN a)
          FILTER LENGTH(shared) > 1
          RETURN KEEP(right, shared) )
      FILTER LENGTH(toZip) > 0
      RETURN ZIP([left._id], [toZip])`;
  return db._query(query, bindVars).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_count_common_properties
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._countCommonProperties = function (vertex1Example, vertex2Example, options) {
  options = options || {};
  if (options.hasOwnProperty('ignoreProperties')) {
    if (!Array.isArray(options.ignoreProperties)) {
      options.ignoreProperties = [options.ignoreProperties];
    }
  }
  var bindVars = {};
  var query = `
    ${generateWithStatement(this, options)}
    ${transformExampleToAQL(vertex1Example, Object.keys(this.__vertexCollections), bindVars, "left")}
      SORT left._id
      LET s = SUM(
        ${transformExampleToAQL(vertex2Example, Object.keys(this.__vertexCollections), bindVars, "right")}
        FILTER right != left
        LET shared = (FOR a IN ATTRIBUTES(left) FILTER a != "_rev" FILTER
          (${options.hasOwnProperty("ignoreProperties") ? `a NOT IN ${JSON.stringify(options.ignoreProperties)} AND` : ""} left[a] == right[a])
          OR a == '_id' RETURN a)
          FILTER LENGTH(shared) > 1
          RETURN 1 )
      FILTER s > 0
      RETURN ZIP([left._id], [s])`;
  return db._query(query, bindVars).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_paths
// //////////////////////////////////////////////////////////////////////////////
Graph.prototype._paths = function (options) {
  options = options || {};

  var query = `
    ${generateWithStatement(this, options)}
    FOR source IN ${startInAllCollections(Object.keys(this.__vertexCollections))}
    FOR v, e, p IN ${options.minLength || 0}..${options.maxLength || 10} ${options.direction || "OUTBOUND"} source GRAPH @graphName `;
  if (options.followCycles) {
    query += `OPTIONS {uniqueEdges: "none"} `;
  } else {
    query += `OPTIONS {uniqueVertices: "path"} `;
  }
  query += `RETURN {source: source, destination: v, edges: p.edges, vertice: p.vertices}`;

  var bindVars = {
    'graphName': this.__name
  };
  return db._query(query, bindVars).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_shortest_path
// //////////////////////////////////////////////////////////////////////////////
Graph.prototype._shortestPath = function (startVertexExample, endVertexExample, options) {
  var bindVars = {};
  options = options || {};
  var query = `
    ${generateWithStatement(this, options)}
    ${transformExampleToAQL(startVertexExample, Object.keys(this.__vertexCollections), bindVars, "start")}
      ${transformExampleToAQL(endVertexExample, Object.keys(this.__vertexCollections), bindVars, "target")}
      FILTER target._id != start._id
        LET p = (FOR v, e IN `;
  if (options.direction === 'outbound') {
    query += 'OUTBOUND ';
  } else if (options.direction === 'inbound') {
    query += 'INBOUND ';
  } else {
    query += 'ANY ';
  }
  query += `SHORTEST_PATH start TO target GRAPH @graphName `;
  fixWeight(options);
  if (options.hasOwnProperty('weightAttribute') && options.hasOwnProperty('defaultWeight')) {
    query += `OPTIONS {weightAttribute: @attribute, defaultWeight: @default}
    RETURN {
      v: v,
      e: e,
      d: IS_NULL(e) ? 0 : (IS_NUMBER(e[@attribute]) ? e[@attribute] : @default)
    }) `;
    bindVars.attribute = options.weightAttribute;
    bindVars.default = options.defaultWeight;
  } else {
    query += 'RETURN {v: v, e: e, d: IS_NULL(e) ? 0 : 1}) ';
  }
  query += `
  FILTER LENGTH(p) > 0`;
  if (options.stopAtFirstMatch) {
    query += `SORT SUM(p[*].d) LIMIT 1 `;
  }
  query += `
  RETURN {
    vertices: p[*].v._id,
    edges: p[* FILTER CURRENT.e != null].e,
    distance: SUM(p[*].d)
  }`;

  bindVars.graphName = this.__name;
  return db._query(query, bindVars).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_distance_to
// //////////////////////////////////////////////////////////////////////////////
Graph.prototype._distanceTo = function (startVertexExample, endVertexExample, options) {
  var bindVars = {};
  options = options || {};
  var query = `
    ${generateWithStatement(this, options)}
    ${transformExampleToAQL(startVertexExample, Object.keys(this.__vertexCollections), bindVars, "start")}
      ${transformExampleToAQL(endVertexExample, Object.keys(this.__vertexCollections), bindVars, "target")}
      FILTER target._id != start._id
        LET p = (FOR v, e IN `;
  if (options.direction === 'outbound') {
    query += 'OUTBOUND ';
  } else if (options.direction === 'inbound') {
    query += 'INBOUND ';
  } else {
    query += 'ANY ';
  }
  query += `SHORTEST_PATH start TO target GRAPH @graphName `;
  fixWeight(options);
  if (options.hasOwnProperty('weightAttribute') && options.hasOwnProperty('defaultWeight')) {
    query += `OPTIONS {weightAttribute: @attribute, defaultWeight: @default}
              FILTER e != null RETURN IS_NUMBER(e[@attribute]) ? e[@attribute] : @default) `;
    bindVars.attribute = options.weightAttribute;
    bindVars.default = options.defaultWeight;
  } else {
    query += 'FILTER e != null RETURN 1) ';
  }
  query += `
  FILTER LENGTH(p) > 0
  RETURN {
    startVertex: start._id,
    vertex: target._id,
    distance: SUM(p)
  }`;

  bindVars.graphName = this.__name;
  return db._query(query, bindVars).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_absolute_eccentricity
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._absoluteEccentricity = function (vertexExample, options) {
  var bindVars = {};
  options = options || {};
  var query = transformExampleToAQL(vertexExample, Object.keys(this.__vertexCollections), bindVars, 'start');
  query += `
  LET lsp = (
    FOR target IN ${startInAllCollections(Object.keys(this.__vertexCollections))}
      FILTER target._id != start._id
        LET p = (FOR v, e IN `;
  if (options.direction === 'outbound') {
    query += 'OUTBOUND ';
  } else if (options.direction === 'inbound') {
    query += 'INBOUND ';
  } else {
    query += 'ANY ';
  }
  query += 'SHORTEST_PATH start TO target GRAPH @graphName ';
  fixWeight(options);
  if (options.hasOwnProperty('weightAttribute') && options.hasOwnProperty('defaultWeight')) {
    query += `OPTIONS {weightAttribute: @attribute, defaultWeight: @default}
              FILTER e != null RETURN IS_NUMBER(e[@attribute]) ? e[@attribute] : @default) `;
    bindVars.attribute = options.weightAttribute;
    bindVars.default = options.defaultWeight;
  } else {
    query += 'FILTER e != null RETURN 1) ';
  }
  query += `LET k = LENGTH(p) == 0 ? 0 : SUM(p) SORT k DESC LIMIT 1 RETURN k)
  RETURN [start._id, lsp[0]]
  `;
  bindVars.graphName = this.__name;
  var cursor = db._query(query, bindVars);
  var result = {};
  while (cursor.hasNext()) {
    var r = cursor.next();
    result[r[0]] = r[1];
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_absolute_closeness
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._farness = Graph.prototype._absoluteCloseness = function (vertexExample, options) {
  var bindVars = {};
  options = options || {};
  var query = transformExampleToAQL(vertexExample, Object.keys(this.__vertexCollections), bindVars, 'start');
  query += `
  LET lsp = (
    FOR target IN ${startInAllCollections(Object.keys(this.__vertexCollections))}
      FILTER target._id != start._id
        LET p = (FOR v, e IN `;
  if (options.direction === 'outbound') {
    query += 'OUTBOUND ';
  } else if (options.direction === 'inbound') {
    query += 'INBOUND ';
  } else {
    query += 'ANY ';
  }
  query += 'SHORTEST_PATH start TO target GRAPH @graphName ';
  fixWeight(options);
  if (options.hasOwnProperty('weightAttribute') && options.hasOwnProperty('defaultWeight')) {
    query += `OPTIONS {weightAttribute: @attribute, defaultWeight: @default}
              FILTER e != null RETURN IS_NUMBER(e[@attribute]) ? e[@attribute] : @default) `;
    bindVars.attribute = options.weightAttribute;
    bindVars.default = options.defaultWeight;
  } else {
    query += 'FILTER e != null RETURN 1) ';
  }
  query += `LET k = LENGTH(p) == 0 ? 0 : SUM(p) RETURN k)
  RETURN [start._id, SUM(lsp)]
  `;
  bindVars.graphName = this.__name;
  var cursor = db._query(query, bindVars);
  var result = {};
  while (cursor.hasNext()) {
    var r = cursor.next();
    result[r[0]] = r[1];
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_eccentricity
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._eccentricity = function (options) {
  let result = this._absoluteEccentricity({}, options);
  let min = Number.POSITIVE_INFINITY;
  for (let k of Object.keys(result)) {
    if (result[k] !== 0 && result[k] < min) {
      min = result[k];
    }
  }
  for (let k of Object.keys(result)) {
    if (result[k] !== 0) {
      result[k] = min / result[k];
    }
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_closeness
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._closeness = function (options) {
  var farness = this._farness({}, options);
  var keys = Object.keys(farness);
  var min = Number.POSITIVE_INFINITY;
  for (let t of keys) {
    if (farness[t] > 0 && farness[t] < min) {
      min = farness[t];
    }
  }
  for (let k of keys) {
    if (farness[k] > 0) {
      farness[k] = min / farness[k];
    }
  }
  return farness;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_absolute_betweenness
// //////////////////////////////////////////////////////////////////////////////
Graph.prototype._absoluteBetweenness = function (example, options) {
  var bindVars = {};
  options = options || {};
  bindVars.graphName = this.__name;

  var query = `
    LET toFind = (${transformExampleToAQL(example, Object.keys(this.__vertexCollections), bindVars, "start")} RETURN start._id)
    LET paths = (
    FOR start IN ${startInAllCollections(Object.keys(this.__vertexCollections))}
      FOR target IN ${startInAllCollections(Object.keys(this.__vertexCollections))}
        FILTER start._id != target._id
        FOR v IN `;
  if (options.direction === 'outbound') {
    query += 'OUTBOUND ';
  } else if (options.direction === 'inbound') {
    query += 'INBOUND ';
  } else {
    query += 'ANY ';
  }
  query += 'SHORTEST_PATH start TO target GRAPH @graphName ';
  fixWeight(options);
  if (options.hasOwnProperty('weightAttribute') && options.hasOwnProperty('defaultWeight')) {
    query += `OPTIONS {weightAttribute: @attribute, defaultWeight: @default} `;
    bindVars.attribute = options.weightAttribute;
    bindVars.default = options.defaultWeight;
  }
  query += `
    FILTER v._id != start._id AND v._id != target._id AND v._id IN toFind
    COLLECT id = v._id WITH COUNT INTO betweenness
    RETURN [id, betweenness])
  RETURN {toFind, paths}
  `;

  var res = db._query(query, bindVars).toArray();
  var result = {};
  var toFind = res[0].toFind;
  for (let pair of res[0].paths) {
    if (options.direction !== 'inbound' && options.direction !== 'outbound') {
      // In any every path is contained twice, once forward once backward.
      result[pair[0]] = pair[1] / 2;
    } else {
      result[pair[0]] = pair[1];
    }
  }
  // Add all not found values as 0.
  for (let nf of toFind) {
    if (!result.hasOwnProperty(nf)) {
      result[nf] = 0;
    }
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_betweenness
// //////////////////////////////////////////////////////////////////////////////
Graph.prototype._betweenness = function (options) {
  let result = this._absoluteBetweenness({}, options);
  let max = 0;
  for (let k of Object.keys(result)) {
    if (result[k] > max) {
      max = result[k];
    }
  }
  if (max !== 0) {
    for (let k of Object.keys(result)) {
      result[k] = result[k] / max;
    }
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_radius
// //////////////////////////////////////////////////////////////////////////////
Graph.prototype._radius = function (options) {
  var vcs = Object.keys(this.__vertexCollections);
  var query = '';
  var ids;
  var bindVars = {
    'graphName': this.__name
  };
  options = options || {};
  if (vcs.length === 1) {
    ids = vcs[0];
  } else {
    query = `LET ids = UNION(${vcs.map(function(v) {return `(FOR x IN ${v} RETURN x)`;}).join(",")}) `;
    ids = 'ids';
  }

  query += `FOR s IN ${ids} LET lsp = (
    FOR t IN ${ids} FILTER s._id != t._id LET p = (FOR v, e IN `;

  if (options.direction === 'outbound') {
    query += 'OUTBOUND ';
  } else if (options.direction === 'inbound') {
    query += 'INBOUND ';
  } else {
    query += 'ANY ';
  }
  query += 'SHORTEST_PATH s TO t GRAPH @graphName ';
  fixWeight(options);
  if (options.hasOwnProperty('weightAttribute') && options.hasOwnProperty('defaultWeight')) {
    query += `OPTIONS {weightAttribute: @attribute, defaultWeight: @default}
              FILTER e != null RETURN IS_NUMBER(e[@attribute]) ? e[@attribute] : @default) `;
    bindVars.attribute = options.weightAttribute;
    bindVars.default = options.defaultWeight;
  } else {
    query += 'FILTER e != null RETURN 1) ';
  }
  query += `FILTER LENGTH(p) > 0 LET k = SUM(p) SORT k DESC LIMIT 1 RETURN k)
            FILTER LENGTH(lsp) != 0
            SORT lsp[0] ASC LIMIT 1 RETURN lsp[0]`;
  var res = db._query(query, bindVars).toArray();
  if (res.length > 0) {
    return res[0];
  }
  return res;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_diameter
// //////////////////////////////////////////////////////////////////////////////
Graph.prototype._diameter = function (options) {
  var vcs = Object.keys(this.__vertexCollections);
  var query;
  if (vcs.length === 1) {
    query = `FOR s IN ${vcs[0]} FOR t IN ${vcs[0]} `;
  } else {
    query = `LET ids = UNION(${vcs.map(function(v) {return `(FOR x IN ${v} RETURN x)`;}).join(",")})
               FOR s IN ids FOR t IN ids `;
  }
  options = options || {};
  if (options.direction === 'outbound') {
    query += 'FILTER s._id != t._id LET p = SUM((FOR v, e IN OUTBOUND ';
  } else if (options.direction === 'inbound') {
    query += 'FILTER s._id != t._id LET p = SUM((FOR v, e IN INBOUND ';
  } else {
    query += 'FILTER s._id < t._id LET p = SUM((FOR v, e IN ANY ';
  }
  var bindVars = {
    'graphName': this.__name
  };
  query += 'SHORTEST_PATH s TO t GRAPH @graphName ';
  fixWeight(options);
  if (options.hasOwnProperty('weightAttribute') && options.hasOwnProperty('defaultWeight')) {
    query += `OPTIONS {weightAttribute: @attribute, defaultWeight: @default}
              FILTER e != null RETURN IS_NUMBER(e[@attribute]) ? e[@attribute] : @default)) `;
    bindVars.attribute = options.weightAttribute;
    bindVars.default = options.defaultWeight;
  } else {
    query += 'RETURN 1)) - 1 ';
  }
  query += 'SORT p DESC LIMIT 1 RETURN p';
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__extendEdgeDefinitions
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._extendEdgeDefinitions = function (edgeDefinition) {
  edgeDefinition = sortEdgeDefinition(edgeDefinition);
  var self = this;
  var err;
  // check if edgeCollection not already used
  var eC = edgeDefinition.collection;
  // ... in same graph
  if (this.__edgeCollections[eC] !== undefined) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.message;
    throw err;
  }
  // in different graph
  db._graphs.toArray().forEach(
    function (singleGraph) {
      var sGEDs = singleGraph.edgeDefinitions;
      sGEDs.forEach(
        function (sGED) {
          var col = sGED.collection;
          if (col === eC) {
            if (JSON.stringify(sGED) !== JSON.stringify(edgeDefinition)) {
              err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code;
              err.errorMessage = col
              + ' ' + arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message;
              throw err;
            }
          }
        }
      );
    }
  );

  findOrCreateCollectionsByEdgeDefinitions([edgeDefinition]);

  this.__edgeDefinitions.push(edgeDefinition);
  db._graphs.update(this.__name, {edgeDefinitions: this.__edgeDefinitions});
  this.__edgeCollections[edgeDefinition.collection] = db[edgeDefinition.collection];
  edgeDefinition.from.forEach(
    function (vc) {
      self[vc] = db[vc];
      // remove from __orphanCollections
      var orphanIndex = self.__orphanCollections.indexOf(vc);
      if (orphanIndex !== -1) {
        self.__orphanCollections.splice(orphanIndex, 1);
      }
      // push into __vertexCollections
      if (self.__vertexCollections[vc] === undefined) {
        self.__vertexCollections[vc] = db[vc];
      }
    }
  );
  edgeDefinition.to.forEach(
    function (vc) {
      self[vc] = db[vc];
      // remove from __orphanCollections
      var orphanIndex = self.__orphanCollections.indexOf(vc);
      if (orphanIndex !== -1) {
        self.__orphanCollections.splice(orphanIndex, 1);
      }
      // push into __vertexCollections
      if (self.__vertexCollections[vc] === undefined) {
        self.__vertexCollections[vc] = db[vc];
      }
    }
  );
  updateBindCollections(this);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal function for editing edge definitions
// //////////////////////////////////////////////////////////////////////////////

var changeEdgeDefinitionsForGraph = function (graph, edgeDefinition, newCollections, possibleOrphans, self) {
  var graphCollections = [];
  var graphObj = _graph(graph._key);
  var eDs = graph.edgeDefinitions;
  var gotAHit = false;

  // replace edgeDefintion
  eDs.forEach(
    function (eD, id) {
      if (eD.collection === edgeDefinition.collection) {
        gotAHit = true;
        eDs[id].from = edgeDefinition.from;
        eDs[id].to = edgeDefinition.to;
        db._graphs.update(graph._key, {edgeDefinitions: eDs});
        if (graph._key === self.__name) {
          self.__edgeDefinitions[id].from = edgeDefinition.from;
          self.__edgeDefinitions[id].to = edgeDefinition.to;
        }
      } else {
        // collect all used collections
        graphCollections = _.union(graphCollections, eD.from);
        graphCollections = _.union(graphCollections, eD.to);
      }
    }
  );
  if (!gotAHit) {
    return;
  }

  // remove used collection from orphanage
  if (graph._key === self.__name) {
    newCollections.forEach(
      function (nc) {
        if (self.__vertexCollections[nc] === undefined) {
          self.__vertexCollections[nc] = db[nc];
        }
        try {
          self._removeVertexCollection(nc, false);
        } catch (ignore) {}
      }
    );
    possibleOrphans.forEach(
      function (po) {
        if (graphCollections.indexOf(po) === -1) {
          delete self.__vertexCollections[po];
          self._addVertexCollection(po);
        }
      }
    );
  } else {
    newCollections.forEach(
      function (nc) {
        try {
          graphObj._removeVertexCollection(nc, false);
        } catch (ignore) {}
      }
    );
    possibleOrphans.forEach(
      function (po) {
        if (graphCollections.indexOf(po) === -1) {
          delete graphObj.__vertexCollections[po];
          graphObj._addVertexCollection(po);
        }
      }
    );
  }

// move unused collections to orphanage
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__editEdgeDefinition
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._editEdgeDefinitions = function (edgeDefinition) {
  edgeDefinition = sortEdgeDefinition(edgeDefinition);
  var self = this;

  // check, if in graphs edge definition
  if (this.__edgeCollections[edgeDefinition.collection] === undefined) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message;
    throw err;
  }

  findOrCreateCollectionsByEdgeDefinitions([edgeDefinition]);

  // evaluate collections to add to orphanage
  var possibleOrphans = [];
  var currentEdgeDefinition;
  this.__edgeDefinitions.forEach(
    function (ed) {
      if (edgeDefinition.collection === ed.collection) {
        currentEdgeDefinition = ed;
      }
    }
  );

  var currentCollections = _.union(currentEdgeDefinition.from, currentEdgeDefinition.to);
  var newCollections = _.union(edgeDefinition.from, edgeDefinition.to);
  currentCollections.forEach(
    function (colName) {
      if (newCollections.indexOf(colName) === -1) {
        possibleOrphans.push(colName);
      }
    }
  );
  // change definition for ALL graphs
  var graphs = getGraphCollection().toArray();
  graphs.forEach(
    function (graph) {
      changeEdgeDefinitionsForGraph(graph, edgeDefinition, newCollections, possibleOrphans, self);
    }
  );
  updateBindCollections(this);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__deleteEdgeDefinition
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._deleteEdgeDefinition = function (edgeCollection, dropCollection) {

  // check, if in graphs edge definition
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
    function (edgeDefinition, idx) {
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
    function (po) {
      if (usedVertexCollections.indexOf(po) === -1) {
        self.__orphanCollections.push(po);
      }
    }
  );

  updateBindCollections(this);
  db._graphs.update(
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__addVertexCollection
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._addVertexCollection = function (vertexCollectionName, createCollection) {
  // check edgeCollection
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
  if (_.includes(this.__orphanCollections, vertexCollectionName)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.message;
    throw err;
  }
  this.__orphanCollections.push(vertexCollectionName);
  updateBindCollections(this);
  db._graphs.update(this.__name, {orphanCollections: this.__orphanCollections});
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__orphanCollections
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._orphanCollections = function () {
  return this.__orphanCollections;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__removeVertexCollection
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._removeVertexCollection = function (vertexCollectionName, dropCollection) {
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
  db._graphs.update(this.__name, {orphanCollections: this.__orphanCollections});

  if (dropCollection === true) {
    var graphs = getGraphCollection().toArray();
    if (checkIfMayBeDropped(vertexCollectionName, null, graphs)) {
      db._drop(vertexCollectionName);
    }
  }
  updateBindCollections(this);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_connectingEdges
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._getConnectingEdges = function (vertexExample1, vertexExample2, options) {
  options = options || {};
  // TODO
  return [];

/*
var opts = {
  includeData: true
}

if (options.vertex1CollectionRestriction) {
  opts.startVertexCollectionRestriction = options.vertex1CollectionRestriction
}

if (options.vertex2CollectionRestriction) {
  opts.endVertexCollectionRestriction = options.vertex2CollectionRestriction
}

if (options.edgeCollectionRestriction) {
  opts.edgeCollectionRestriction = options.edgeCollectionRestriction
}

if (options.edgeExamples) {
  opts.edgeExamples = options.edgeExamples
}

if (vertexExample2) {
  opts.neighborExamples = vertexExample2
}

var query = "RETURN"
  + " GRAPH_EDGES(@graphName"
  + ',@vertexExample'
  + ',@options'
  + ')'
var bindVars = {
  "graphName": this.__name,
  "vertexExample": vertexExample1,
  "options": opts
}
var result = db._query(query, bindVars).toArray()
return result[0]
*/
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print basic information for the graph
// //////////////////////////////////////////////////////////////////////////////

Graph.prototype._PRINT = function (context) {
  var name = this.__name;
  var edgeDefs = printEdgeDefinitions(this.__edgeDefinitions);
  context.output += '[ Graph ';
  context.output += name;
  context.output += ' EdgeDefinitions: ';
  internal.printRecursive(edgeDefs, context);
  context.output += ' VertexCollections: ';
  internal.printRecursive(this.__orphanCollections, context);
  context.output += ' ]';
};

exports._relation = _relation;
exports._graph = _graph;
exports._edgeDefinitions = _edgeDefinitions;
exports._extendEdgeDefinitions = _extendEdgeDefinitions;
exports._create = _create;
exports._drop = _drop;
exports._exists = _exists;
exports._renameCollection = _renameCollection;
exports._list = _list;
exports._listObjects = _listObjects;
exports._registerCompatibilityFunctions = registerCompatibilityFunctions;

// //////////////////////////////////////////////////////////////////////////////
// / some more documentation
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_create_graph_example1
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_create_graph_example2
// //////////////////////////////////////////////////////////////////////////////
