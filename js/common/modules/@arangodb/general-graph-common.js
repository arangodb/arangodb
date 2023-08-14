/* jshint strict: false */

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

const arangodb = require('@arangodb');
const ArangoCollection = arangodb.ArangoCollection;
const ArangoError = arangodb.ArangoError;
const db = arangodb.db;
const users = require('@arangodb/users');
const _ = require('lodash');

let fixWeight = function (options) {
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

var checkROPermission = function(c) {
  if (!users.isAuthActive()) {
    return;
  }

  let user = users.currentUser();
  if (user) {
    let p = users.permission(user, db._name(), c);
    var err = new ArangoError();
    if (p === 'none') {
      err.errorNum = arangodb.errors.ERROR_FORBIDDEN.code;
      err.errorMessage = arangodb.errors.ERROR_FORBIDDEN.message;
      throw err;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief find or create a collection by name
// //////////////////////////////////////////////////////////////////////////////

var findCollectionByName = function (name, type) {
  let col = db._collection(name);
  let res = false;
  if (!(col instanceof ArangoCollection)) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code;
    err.errorMessage = name + arangodb.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.message;
    throw err;
  } else if (type === ArangoCollection.TYPE_EDGE && col.type() !== type) {
    var err2 = new ArangoError();
    err2.errorNum = arangodb.errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code;
    err2.errorMessage = name + ' cannot be used as relation. It is not an edge collection';
    throw err2;
  }
  checkROPermission(name);
  return res;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief find or create a collection by name
// //////////////////////////////////////////////////////////////////////////////

var findCollectionsByEdgeDefinitions = function (edgeDefinitions) {
  let vertexCollections = {};
  let edgeCollections = {};
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

    e.from.forEach(function (v) {
      findCollectionByName(v, ArangoCollection.TYPE_DOCUMENT);
      vertexCollections[v] = db[v];
    });
    e.to.forEach(function (v) {
      findCollectionByName(v, ArangoCollection.TYPE_DOCUMENT);
      vertexCollections[v] = db[v];
    });

    findCollectionByName(e.collection, ArangoCollection.TYPE_EDGE);
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
  return `UNION(${collections.map(c => `(FOR x IN ${c} RETURN x)`).join(', ')})`;
};

var buildEdgeCollectionRestriction = function (collections, bindVars, graph) {
  if (typeof collections === 'string') {
    collections = [collections];
  }
  if (!Array.isArray(collections) || collections.length === 0) {
    bindVars.graphName = graph.__name;
    return 'GRAPH @graphName';
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
    }).join(' OR ')}
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
      if (typeof e === 'object') {
        var keys = Object.keys(e);
        if (keys.length === 0) {
          foundAllMatch = true;
          return '';
        }
        return keys.map(key => {
          bindVars[varname + 'ExVar' + varcount] = key;
          bindVars[varname + 'ExVal' + varcount] = e[key];
          return `${varname}[@${varname}ExVar${varcount}] == @${varname}ExVal${varcount++}`;
        }).join(' AND ');
      } else if (typeof e === 'string') {
        bindVars[varname + 'ExVar' + varcount] = e;
        return `${varname}._id == @${varname}ExVar${varcount++}`;
      } else {
        foundAllMatch = true;
        return '';
      }
    }).join(') OR (')}
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
  if (!options.hasOwnProperty('edgeCollectionRestriction') ||
    !Array.isArray(options.edgeCollectionRestriction) ||
    options.edgeCollectionRestriction.length === 0) {
    return '';
  }
  return 'WITH ' + Object.keys(graph.__vertexCollections).join(', ');
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
      if (typeof e === 'object') {
        var keys = Object.keys(e);
        if (keys.length === 0) {
          foundAllMatch = true;
          return '';
        }
        return keys.map(key => {
          bindVars[varname + 'ExVar' + varcount] = key;
          bindVars[varname + 'ExVal' + varcount] = e[key];
          return `${varname}[@${varname}ExVar${varcount}] == @${varname}ExVal${varcount++}`;
        }).join(' AND ');
      } else {
        bindVars[varname + 'ExVar' + varcount] = e;
        return `${varname}._id == @${varname}ExVar${varcount++}`;
      }
    }).join(') OR (')}
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
    query += `UNION (${collections.map(c => `(FOR ${varname} IN ${c} ${filter} RETURN ${varname})`).join(', ')}) `;
  }
  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / internal helper to sort a graph's edge definitions
// //////////////////////////////////////////////////////////////////////////////

var sortEdgeDefinitionInplace = function (edgeDefinition) {
  edgeDefinition.from.sort();
  edgeDefinition.to.sort();
  return edgeDefinition;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief create a new graph
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_how_to_create
// //////////////////////////////////////////////////////////////////////////////

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
    var oldSave = wrap.insert;
    wrap.save = wrap.insert = function (from, to, data) {
      var options = {};
      if (typeof from === 'object' && to === undefined) {
        data = from;
        from = data._from;
        to = data._to;
      } else if (typeof from === 'object' && typeof to === 'object' && data === undefined) {
        data = from;
        options = to;
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
            if (!_.includes(edgeDefinition.from, fromCollection) ||
              !_.includes(edgeDefinition.to, toCollection)) {
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
      return oldSave(data, options);
    };

    // remove
    wrap.remove = function (edgeId, options) {
      // if _key make _id (only on 1st call)
      if (edgeId.indexOf('/') === -1) {
        edgeId = key + '/' + edgeId;
      }
      var graphs = exports._listObjects();
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
      var graphs = exports._listObjects();
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
                if (from.indexOf(vertexCollectionName) !== -1 ||
                  to.indexOf(vertexCollectionName) !== -1
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

// @brief Class Graph. Defines a graph in the Database.
class AbstractGraph {
  constructor(info) {
    // We assume well-formedness of the input.
    // User cannot directly call this constructor.
    let vertexCollections = {};
    let edgeCollections = {};

    info.edgeDefinitions.forEach((e) => {
      // Link the edge collection
      edgeCollections[e.collection] = db._collection(e.collection);
      // Link from collections
      e.from.forEach((v) => {
        vertexCollections[v] = db._collection(v);
      });
      // Link to collections
      e.to.forEach((v) => {
        vertexCollections[v] = db._collection(v);
      });
    });

    // we can call the "fast" version of some edge functions if we are
    // running server-side and are not a coordinator
    let useBuiltIn = require('internal').isArangod();
    if (useBuiltIn && require('@arangodb/cluster').isCoordinator()) {
      useBuiltIn = false;
    }

    let self = this;
    // Create Hidden Properties
    createHiddenProperty(this, '__useBuiltIn', useBuiltIn);
    createHiddenProperty(this, '__name', info._key || info.name);
    createHiddenProperty(this, '__vertexCollections', vertexCollections);
    createHiddenProperty(this, '__edgeCollections', edgeCollections);
    createHiddenProperty(this, '__edgeDefinitions', info.edgeDefinitions);
    createHiddenProperty(this, '__idsToRemove', {});
    createHiddenProperty(this, '__collectionsToLock', {});
    createHiddenProperty(this, '__id', info._id);
    createHiddenProperty(this, '__rev', info._rev);
    createHiddenProperty(this, '__orphanCollections', info.orphanCollections || []);

    if (info.numberOfShards) {
      createHiddenProperty(this, '__numberOfShards', info.numberOfShards);
    }
    createHiddenProperty(this, '__replicationFactor', info.replicationFactor || 1);

    // Create Hidden Functions
    createHiddenProperty(this, '__updateBindCollections', updateBindCollections);
    createHiddenProperty(this, '__sortEdgeDefinition', sortEdgeDefinitionInplace);
    updateBindCollections(self);
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief return all edge collections of the graph.
// //////////////////////////////////////////////////////////////////////////////

  _edgeCollections () {
    return _.values(this.__edgeCollections);
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief return all vertex collections of the graph.
// //////////////////////////////////////////////////////////////////////////////

  _vertexCollections(excludeOrphans) {
    if (excludeOrphans) {
      return this.__vertexCollections;
    }
    var orphans = [];
    _.each(this.__orphanCollections, function (o) {
      orphans.push(db[o]);
    });
    return _.union(_.values(this.__vertexCollections), orphans);
  }

  __updateDefinitions(edgeDefs, orphans) {
    this.__edgeDefinitions = edgeDefs;
    this.__orphanCollections = orphans;
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief _EDGES(vertexId).
// //////////////////////////////////////////////////////////////////////////////

  // might be needed from AQL itself
  _EDGES(vertexId) {
    if (vertexId.indexOf('/') === -1) {
      let err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
      err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ': ' + vertexId;
      throw err;
    }

    let result = [];
    for (let c in this.__edgeCollections) {
      if (this.__edgeCollections.hasOwnProperty(c)) {
        if (this.__useBuiltIn) {
          result = result.concat(this.__edgeCollections[c].EDGES(vertexId));
        } else {
          result = result.concat(this.__edgeCollections[c].edges(vertexId));
        }
      }
    }
    return result;
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief INEDGES(vertexId).
// //////////////////////////////////////////////////////////////////////////////

  _INEDGES (vertexId) {
    if (vertexId.indexOf('/') === -1) {
      let err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
      err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ': ' + vertexId;
      throw err;
    }

    let result = [];
    for (let c in this.__edgeCollections) {
      if (this.__edgeCollections.hasOwnProperty(c)) {
        if (this.__useBuiltIn) {
          result = result.concat(this.__edgeCollections[c].INEDGES(vertexId));
        } else {
          result = result.concat(this.__edgeCollections[c].inEdges(vertexId));
        }
      }
    }
    return result;
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief outEdges(vertexId).
// //////////////////////////////////////////////////////////////////////////////

  _OUTEDGES (vertexId) {
    if (vertexId.indexOf('/') === -1) {
      let err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
      err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ': ' + vertexId;
      throw err;
    }

    let result = [];
    for (let c in this.__edgeCollections) {
      if (this.__edgeCollections.hasOwnProperty(c)) {
        if (this.__useBuiltIn) {
          result = result.concat(this.__edgeCollections[c].OUTEDGES(vertexId));
        } else {
          result = result.concat(this.__edgeCollections[c].outEdges(vertexId));
        }
      }
    }
    return result;
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_edges
// //////////////////////////////////////////////////////////////////////////////

  _edges (vertexExample, options) {
    var bindVars = {};
    options = options || {};
    var query = `
      ${transformExampleToAQL(vertexExample, Object.keys(this.__vertexCollections), bindVars, 'start')}
      FOR v, e IN ${options.minDepth || 1}..${options.maxDepth || 1} ${options.direction || 'ANY'} start
      ${buildEdgeCollectionRestriction(options.edgeCollectionRestriction, bindVars, this)}
      ${buildFilter(options.edgeExamples, bindVars, 'e')}
      RETURN DISTINCT ${options.includeData === true ? 'e' : 'e._id'}`;
    return db._query(query, bindVars).toArray();
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_vertices
// //////////////////////////////////////////////////////////////////////////////

  _vertices (vertexExample, options) {
    options = options || {};
    if (options.vertexCollectionRestriction) {
      if (!Array.isArray(options.vertexCollectionRestriction)) {
        options.vertexCollectionRestriction = [ options.vertexCollectionRestriction ];
      }
    }
    var bindVars = {};
    var query = `${transformExampleToAQL({}, Array.isArray(options.vertexCollectionRestriction) && options.vertexCollectionRestriction.length > 0 ? options.vertexCollectionRestriction : Object.keys(this.__vertexCollections), bindVars, 'start')}
    ${buildFilter(vertexExample, bindVars, 'start')}
    RETURN DISTINCT start`;
    return db._query(query, bindVars).toArray();
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_fromVertex
// //////////////////////////////////////////////////////////////////////////////

  _fromVertex (edgeId) {
    if (typeof edgeId !== 'string' ||
      edgeId.indexOf('/') === -1) {
      let err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
      err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
      throw err;
    }
    let edgeCollection = this._getEdgeCollectionByName(edgeId.split('/')[0]);
    let document = edgeCollection.document(edgeId);
    if (document) {
      let vertexId = document._from;
      let vertexCollection = this._getVertexCollectionByName(vertexId.split('/')[0]);
      return vertexCollection.document(vertexId);
    }
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_toVertex
// //////////////////////////////////////////////////////////////////////////////

  _toVertex (edgeId) {
    if (typeof edgeId !== 'string' ||
      edgeId.indexOf('/') === -1) {
      let err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
      err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
      throw err;
    }
    let edgeCollection = this._getEdgeCollectionByName(edgeId.split('/')[0]);
    let document = edgeCollection.document(edgeId);
    if (document) {
      let vertexId = document._to;
      let vertexCollection = this._getVertexCollectionByName(vertexId.split('/')[0]);
      return vertexCollection.document(vertexId);
    }
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief get edge collection by name.
// //////////////////////////////////////////////////////////////////////////////

  _getEdgeCollectionByName (name) {
    if (this.__edgeCollections[name]) {
      return this.__edgeCollections[name];
    }
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.message + ': ' + name;
    throw err;
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief get vertex collection by name.
// //////////////////////////////////////////////////////////////////////////////

  _getVertexCollectionByName (name) {
    if (this.__vertexCollections[name]) {
      return this.__vertexCollections[name];
    }
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message + ': ' + name;
    throw err;
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_neighbors
// //////////////////////////////////////////////////////////////////////////////

  _neighbors (vertexExample, options) {
    options = options || {};
    if (options.vertexCollectionRestriction) {
      if (!Array.isArray(options.vertexCollectionRestriction)) {
        options.vertexCollectionRestriction = [ options.vertexCollectionRestriction ];
      }
    }

    var bindVars = {};
    var query = `
      ${generateWithStatement(this, options)}
      ${transformExampleToAQL(vertexExample, Object.keys(this.__vertexCollections), bindVars, 'start')}
      FOR v, e IN ${options.minDepth || 1}..${options.maxDepth || 1} ${options.direction || 'ANY'} start
      ${buildEdgeCollectionRestriction(options.edgeCollectionRestriction, bindVars, this)}
      OPTIONS {bfs: true}
      ${buildFilter(options.neighborExamples, bindVars, 'v')}
      ${buildFilter(options.edgeExamples, bindVars, 'e')}
      ${Array.isArray(options.vertexCollectionRestriction) && options.vertexCollectionRestriction.length > 0 ? buildVertexCollectionRestriction(options.vertexCollectionRestriction, 'v') : ''}
      RETURN DISTINCT ${options.includeData === true ? 'v' : 'v._id'}`;
    return db._query(query, bindVars).toArray();
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_common_neighbors
// //////////////////////////////////////////////////////////////////////////////

  _commonNeighbors (vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {
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
      ${generateWithStatement(this, optionsVertex1.hasOwnProperty('edgeCollectionRestriction') ? optionsVertex1 : optionsVertex2)}
      ${transformExampleToAQL(vertex1Example, Object.keys(this.__vertexCollections), bindVars, 'left')}
        LET leftNeighbors = (FOR v IN ${optionsVertex1.minDepth || 1}..${optionsVertex1.maxDepth || 1} ${optionsVertex1.direction || 'ANY'} left
          ${buildEdgeCollectionRestriction(optionsVertex1.edgeCollectionRestriction, bindVars, this)}
          OPTIONS {bfs: true, uniqueVertices: "global"}
          ${Array.isArray(optionsVertex1.vertexCollectionRestriction) && optionsVertex1.vertexCollectionRestriction.length > 0 ? buildVertexCollectionRestriction(optionsVertex1.vertexCollectionRestriction, 'v') : ''}
          RETURN v)
        ${transformExampleToAQL(vertex2Example, Object.keys(this.__vertexCollections), bindVars, 'right')}
          FILTER right != left
          LET rightNeighbors = (FOR v IN ${optionsVertex2.minDepth || 1}..${optionsVertex2.maxDepth || 1} ${optionsVertex2.direction || 'ANY'} right
          ${buildEdgeCollectionRestriction(optionsVertex2.edgeCollectionRestriction, bindVars, this)}
          OPTIONS {bfs: true, uniqueVertices: "global"}
          ${Array.isArray(optionsVertex2.vertexCollectionRestriction) && optionsVertex2.vertexCollectionRestriction.length > 0 ? buildVertexCollectionRestriction(optionsVertex2.vertexCollectionRestriction, 'v') : ''}
          RETURN v)
          LET neighbors = INTERSECTION(leftNeighbors, rightNeighbors)
          FILTER LENGTH(neighbors) > 0 `;
    if (optionsVertex1.includeData === true || optionsVertex2.includeData === true) {
      query += `RETURN {left : left._id, right: right._id, neighbors: neighbors}`;
    } else {
      query += `RETURN {left : left._id, right: right._id, neighbors: neighbors[*]._id}`;
    }
    return db._query(query, bindVars).toArray();
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_count_common_neighbors
// //////////////////////////////////////////////////////////////////////////////

  _countCommonNeighbors (vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {
    let result = this._commonNeighbors(vertex1Example, vertex2Example, optionsVertex1, optionsVertex2);
    let tmp = {};
    let tmp2 = {};
    let returnHash = [];
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_common_properties
// //////////////////////////////////////////////////////////////////////////////

  _commonProperties (vertex1Example, vertex2Example, options) {
    options = options || {};
    if (options.hasOwnProperty('ignoreProperties')) {
      if (!Array.isArray(options.ignoreProperties)) {
        options.ignoreProperties = [options.ignoreProperties];
      }
    }
    var bindVars = {};
    var query = `
      ${generateWithStatement(this, options)}
      ${transformExampleToAQL(vertex1Example, Object.keys(this.__vertexCollections), bindVars, 'left')}
        SORT left._id
        LET toZip = (
          ${transformExampleToAQL(vertex2Example, Object.keys(this.__vertexCollections), bindVars, 'right')}
          FILTER right != left
          LET shared = (FOR a IN ATTRIBUTES(left) FILTER a != "_rev" FILTER
            (${options.hasOwnProperty('ignoreProperties') ? `a NOT IN ${JSON.stringify(options.ignoreProperties)} AND` : ''} left[a] == right[a])
            OR a == '_id' RETURN a)
            FILTER LENGTH(shared) > 1
            RETURN KEEP(right, shared) )
        FILTER LENGTH(toZip) > 0
        RETURN ZIP([left._id], [toZip])`;
    return db._query(query, bindVars).toArray();
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_count_common_properties
// //////////////////////////////////////////////////////////////////////////////

  _countCommonProperties (vertex1Example, vertex2Example, options) {
    options = options || {};
    if (options.hasOwnProperty('ignoreProperties')) {
      if (!Array.isArray(options.ignoreProperties)) {
        options.ignoreProperties = [options.ignoreProperties];
      }
    }
    var bindVars = {};
    var query = `
      ${generateWithStatement(this, options)}
      ${transformExampleToAQL(vertex1Example, Object.keys(this.__vertexCollections), bindVars, 'left')}
        SORT left._id
        LET s = SUM(
          ${transformExampleToAQL(vertex2Example, Object.keys(this.__vertexCollections), bindVars, 'right')}
          FILTER right != left
          LET shared = (FOR a IN ATTRIBUTES(left) FILTER a != "_rev" FILTER
            (${options.hasOwnProperty('ignoreProperties') ? `a NOT IN ${JSON.stringify(options.ignoreProperties)} AND` : ''} left[a] == right[a])
            OR a == '_id' RETURN a)
            FILTER LENGTH(shared) > 1
            RETURN 1 )
        FILTER s > 0
        RETURN ZIP([left._id], [s])`;
    return db._query(query, bindVars).toArray();
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_paths
// //////////////////////////////////////////////////////////////////////////////

  _paths (options) {
    options = options || {};

    var query = `
      ${generateWithStatement(this, options)}
      FOR source IN ${startInAllCollections(Object.keys(this.__vertexCollections))}
      FOR v, e, p IN ${options.minLength || 0}..${options.maxLength || 10} ${options.direction || 'OUTBOUND'} source GRAPH @graphName `;
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_shortest_path
// //////////////////////////////////////////////////////////////////////////////

  _shortestPath (startVertexExample, endVertexExample, options) {
    var bindVars = {};
    options = options || {};
    var query = `
      ${generateWithStatement(this, options)}
      ${transformExampleToAQL(startVertexExample, Object.keys(this.__vertexCollections), bindVars, 'start')}
        ${transformExampleToAQL(endVertexExample, Object.keys(this.__vertexCollections), bindVars, 'target')}
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_distance_to
// //////////////////////////////////////////////////////////////////////////////

  _distanceTo (startVertexExample, endVertexExample, options) {
    var bindVars = {};
    options = options || {};
    var query = `
      ${generateWithStatement(this, options)}
      ${transformExampleToAQL(startVertexExample, Object.keys(this.__vertexCollections), bindVars, 'start')}
        ${transformExampleToAQL(endVertexExample, Object.keys(this.__vertexCollections), bindVars, 'target')}
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_absolute_eccentricity
// //////////////////////////////////////////////////////////////////////////////

  _absoluteEccentricity (vertexExample, options) {
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_absolute_closeness
// //////////////////////////////////////////////////////////////////////////////

  _farness (vertexExample, options) {
    return this._absoluteCloseness(vertexExample, options);
  }

  _absoluteCloseness (vertexExample, options) {
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_eccentricity
// //////////////////////////////////////////////////////////////////////////////

  _eccentricity (options) {
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_closeness
// //////////////////////////////////////////////////////////////////////////////

  _closeness (options) {
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_absolute_betweenness
// //////////////////////////////////////////////////////////////////////////////
  _absoluteBetweenness (example, options) {
    var bindVars = {};
    options = options || {};
    bindVars.graphName = this.__name;

    var query = `
      LET toFind = (${transformExampleToAQL(example, Object.keys(this.__vertexCollections), bindVars, 'start')} RETURN start._id)
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_betweenness
// //////////////////////////////////////////////////////////////////////////////

  _betweenness (options) {
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_radius
// //////////////////////////////////////////////////////////////////////////////

  _radius (options) {
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
      query = `LET ids = UNION(${vcs.map((v) => `(FOR x IN ${v} RETURN x)`).join(',')}) `;
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_diameter
// //////////////////////////////////////////////////////////////////////////////
  _diameter (options) {
    var vcs = Object.keys(this.__vertexCollections);
    var query;
    if (vcs.length === 1) {
      query = `FOR s IN ${vcs[0]} FOR t IN ${vcs[0]} `;
    } else {
      query = `LET ids = UNION(${vcs.map((v) => `(FOR x IN ${v} RETURN x)`).join(',')})
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__orphanCollections
// //////////////////////////////////////////////////////////////////////////////

  _orphanCollections () {
    return this.__orphanCollections;
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_connectingEdges
// //////////////////////////////////////////////////////////////////////////////

  _getConnectingEdges (vertexExample1, vertexExample2, options) {
    options = options || {};
    if (options.vertex1CollectionRestriction) {
      if (!Array.isArray(options.vertex1CollectionRestriction)) {
        options.vertex1CollectionRestriction = [ options.vertex1CollectionRestriction ];
      }
    }
    if (options.vertex2CollectionRestriction) {
      if (!Array.isArray(options.vertex2CollectionRestriction)) {
        options.vertex2CollectionRestriction = [ options.vertex2CollectionRestriction ];
      }
    }

    /*var query = `
      ${generateWithStatement(this, optionsVertex1.hasOwnProperty('edgeCollectionRestriction') ? optionsVertex1 : optionsVertex2)}
      ${transformExampleToAQL(vertex1Example, Object.keys(this.__vertexCollections), bindVars, 'left')}
        LET leftNeighbors = (FOR v IN ${optionsVertex1.minDepth || 1}..${optionsVertex1.maxDepth || 1} ${optionsVertex1.direction || 'ANY'} left
          ${buildEdgeCollectionRestriction(optionsVertex1.edgeCollectionRestriction, bindVars, this)}
          OPTIONS {bfs: true, uniqueVertices: "global"}
          ${Array.isArray(optionsVertex1.vertexCollectionRestriction) && optionsVertex1.vertexCollectionRestriction.length > 0 ? buildVertexCollectionRestriction(optionsVertex1.vertexCollectionRestriction, 'v') : ''}
          RETURN v)
        ${transformExampleToAQL(vertex2Example, Object.keys(this.__vertexCollections), bindVars, 'right')}
          FILTER right != left
          LET rightNeighbors = (FOR v IN ${optionsVertex2.minDepth || 1}..${optionsVertex2.maxDepth || 1} ${optionsVertex2.direction || 'ANY'} right
          ${buildEdgeCollectionRestriction(optionsVertex2.edgeCollectionRestriction, bindVars, this)}
          OPTIONS {bfs: true, uniqueVertices: "global"}
          ${Array.isArray(optionsVertex2.vertexCollectionRestriction) && optionsVertex2.vertexCollectionRestriction.length > 0 ? buildVertexCollectionRestriction(optionsVertex2.vertexCollectionRestriction, 'v') : ''}
          RETURN v)
          LET neighbors = INTERSECTION(leftNeighbors, rightNeighbors)
          FILTER LENGTH(neighbors) > 0 `;
    if (optionsVertex1.includeData === true || optionsVertex2.includeData === true) {
      query += `RETURN {left : left, right: right, neighbors: neighbors}`;
    } else {
      query += `RETURN {left : left._id, right: right._id, neighbors: neighbors[*]._id}`;
    }
    return db._query(query, bindVars).toArray();*/


    // TODO
    return [];
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief print basic information for the graph
// //////////////////////////////////////////////////////////////////////////////

/*
  _PRINT (context) {
    var name = this.__name;
    var edgeDefs = printEdgeDefinitions(this.__edgeDefinitions);
    context.output += '[ Graph ';
    context.output += name;
    context.output += ' EdgeDefinitions: ';
    internal.printRecursive(edgeDefs, context);
    context.output += ' VertexCollections: ';
    internal.printRecursive(this.__orphanCollections, context);
    context.output += ' ]';
  }
  */
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_relation
// //////////////////////////////////////////////////////////////////////////////

exports._relation = function (relationName, fromVertexCollections, toVertexCollections) {
  if (arguments.length < 3) {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.message + '3';
    throw err;
  }

  if (typeof relationName !== 'string' || relationName === '') {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message + ' arg1 must be non empty string';
    throw err;
  }

  if (!isValidCollectionsParameter(fromVertexCollections)) {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message +
                       ' arg2 must be non empty string or array';
    throw err;
  }

  if (!isValidCollectionsParameter(toVertexCollections)) {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message +
                       ' arg3 must be non empty string or array';
    throw err;
  }

  return {
    collection: relationName,
    from: stringToArray(fromVertexCollections),
    to: stringToArray(toVertexCollections)
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_edge_definitions
// //////////////////////////////////////////////////////////////////////////////

exports._edgeDefinitions = function () {
  let res = [];
  let args = arguments;
  Object.keys(args).forEach(function (x) {
    res.push(args[x]);
  });

  return res;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_extend_edge_definitions
// //////////////////////////////////////////////////////////////////////////////

exports._extendEdgeDefinitions = function (edgeDefinition) {
  let args = arguments;
  let i = 0;

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
// / @brief check if a graph exists.
// //////////////////////////////////////////////////////////////////////////////

exports._exists = function (graphId) {
  var gCol = getGraphCollection();
  return gCol.exists(graphId);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_list
// //////////////////////////////////////////////////////////////////////////////

exports._listObjects = function () {
  return db._query(`FOR x IN _graphs RETURN x`).toArray();
};

exports.__GraphClass = AbstractGraph;
