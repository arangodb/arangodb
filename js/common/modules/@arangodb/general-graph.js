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

const arangodb = require('@arangodb');
const internal = require('internal');
const ArangoCollection = arangodb.ArangoCollection;
const ArangoError = arangodb.ArangoError;
const db = arangodb.db;
const errors = arangodb.errors;
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief find or create a collection by name
// //////////////////////////////////////////////////////////////////////////////

var findOrCreateCollectionByName = function (name, type, noCreate, options) {
  let col = db._collection(name);
  let res = false;
  if (col === null && !noCreate) {
    if (type === ArangoCollection.TYPE_DOCUMENT) {
      if (options) {
        col = db._create(name, options);
      } else {
        col = db._create(name);
      }
    } else {
      if (options) {
        col = db._createEdgeCollection(name, options);
      } else {
        col = db._createEdgeCollection(name);
      }
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

var findOrCreateCollectionsByEdgeDefinitions = function (edgeDefinitions, noCreate, options) {
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
      findOrCreateCollectionByName(v, ArangoCollection.TYPE_DOCUMENT, noCreate, options);
      vertexCollections[v] = db[v];
    });
    e.to.forEach(function (v) {
      findOrCreateCollectionByName(v, ArangoCollection.TYPE_DOCUMENT, noCreate, options);
      vertexCollections[v] = db[v];
    });

    findOrCreateCollectionByName(e.collection, ArangoCollection.TYPE_EDGE, noCreate, options);
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
      return oldSave(data);
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

var checkRWPermission = function (c) {
  if (!users.isAuthActive()) {
    return;
  }

  let user = users.currentUser();
  if (user) {
    let p = users.permission(user, db._name(), c);
    //print(`${user}: ${db._name()}/${c} = ${p}`);
    if (p !== 'rw') {
      //print(`Denied ${user} access to ${db._name()}/${c}`);
      var err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_FORBIDDEN.code;
      err.errorMessage = arangodb.errors.ERROR_FORBIDDEN.message;
      throw err;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal function for editing edge definitions
// //////////////////////////////////////////////////////////////////////////////

var changeEdgeDefinitionsForGraph = function (graph, edgeDefinition, newCollections, possibleOrphans, self) {
  var graphCollections = [];
  var graphObj = exports._graph(graph._key);
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
// / @brief Helper for dropping collections of a graph.
// //////////////////////////////////////////////////////////////////////////////

var checkIfMayBeDropped = function (colName, graphName, graphs) {
  var result = true;

  graphs.forEach(
    function (graph) {
      if (result === false) {
        // Short circuit
        return;
      }
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
            if (collection === colName ||
              from.indexOf(colName) !== -1 ||
              to.indexOf(colName) !== -1
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

// @brief Class Graph. Defines a graph in the Database.
class Graph {
  constructor (info) {
    // We assume well-formedness of the input.
    // User cannot directly call this constructor.
    let vertexCollections = {};
    let edgeCollections = {};

    info.edgeDefinitions.forEach((e) => {
      // Link the edge collection
      edgeCollections[e.collection] = db[e.collection];
      // Link from collections
      e.from.forEach((v) => {
        vertexCollections[v] = db[v];
      });
      // Link to collections
      e.to.forEach((v) => {
        vertexCollections[v] = db[v];
      });
    });

    // we can call the "fast" version of some edge functions if we are
    // running server-side and are not a coordinator
    var useBuiltIn = (typeof ArangoClusterComm === 'object');
    if (useBuiltIn && require('@arangodb/cluster').isCoordinator()) {
      useBuiltIn = false;
    }

    var self = this;
    // Create Hidden Properties
    createHiddenProperty(this, '__useBuiltIn', useBuiltIn);
    createHiddenProperty(this, '__name', info._key);
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
    createHiddenProperty(this, '__sortEdgeDefinition', sortEdgeDefinition);
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

  _vertexCollections (excludeOrphans) {
    if (excludeOrphans) {
      return this.__vertexCollections;
    }
    var orphans = [];
    _.each(this.__orphanCollections, function (o) {
      orphans.push(db[o]);
    });
    return _.union(_.values(this.__vertexCollections), orphans);
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief _EDGES(vertexId).
// //////////////////////////////////////////////////////////////////////////////

  // might be needed from AQL itself
  _EDGES (vertexId) {
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
      query += `RETURN {left : left, right: right, neighbors: neighbors}`;
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
// / @brief was docuBlock JSF_general_graph__extendEdgeDefinitions
// //////////////////////////////////////////////////////////////////////////////

  _extendEdgeDefinitions (edgeDefinition) {
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
                err.errorMessage = col + ' ' +
                                   arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message;
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__editEdgeDefinition
// //////////////////////////////////////////////////////////////////////////////

  _editEdgeDefinitions (edgeDefinition) {
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
    var graphs = exports._listObjects();
    graphs.forEach(
      function (graph) {
        changeEdgeDefinitionsForGraph(graph, edgeDefinition, newCollections, possibleOrphans, self);
      }
    );
    updateBindCollections(this);
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__deleteEdgeDefinition
// //////////////////////////////////////////////////////////////////////////////

  _deleteEdgeDefinition (edgeCollection, dropCollection) {
    // check, if in graphs edge definition
    if (this.__edgeCollections[edgeCollection] === undefined) {
      var err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code;
      err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message;
      throw err;
    }
    if (dropCollection) {
      checkRWPermission(edgeCollection);
    }

    let edgeDefinitions = this.__edgeDefinitions;
    let self = this;
    let usedVertexCollections = [];
    let possibleOrphans = [];
    let index;

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
    let gdb = getGraphCollection();
    gdb.update(
      this.__name,
      {
        orphanCollections: this.__orphanCollections,
        edgeDefinitions: this.__edgeDefinitions
      }
    );

    if (dropCollection) {
      db._drop(edgeCollection);
    }
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__addVertexCollection
// //////////////////////////////////////////////////////////////////////////////

  _addVertexCollection (vertexCollectionName, createCollection) {
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
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__orphanCollections
// //////////////////////////////////////////////////////////////////////////////

  _orphanCollections () {
    return this.__orphanCollections;
  }

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph__removeVertexCollection
// //////////////////////////////////////////////////////////////////////////////

  _removeVertexCollection (vertexCollectionName, dropCollection) {
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

    if (dropCollection) {
      checkRWPermission(vertexCollectionName);
    }

    this.__orphanCollections.splice(index, 1);
    delete this[vertexCollectionName];
    db._graphs.update(this.__name, {orphanCollections: this.__orphanCollections});

    if (dropCollection === true) {
      var graphs = exports._listObjects();
      if (checkIfMayBeDropped(vertexCollectionName, null, graphs)) {
        db._drop(vertexCollectionName);
      }
    }
    updateBindCollections(this);
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
// / @brief was docuBlock JSF_general_graph_graph
// //////////////////////////////////////////////////////////////////////////////

exports._graph = function (graphName) {
  let gdb = getGraphCollection();
  let g;
  let collections;
  let orphanCollections;

  try {
    g = gdb.document(graphName);
  } catch (e) {
    if (e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw e;
    }
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message;
    throw err;
  }
  if (g.isSmart) {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_GRAPH.code;
    err.errorMessage = 'The graph you requested is a SmartGraph (Enterprise Only)';
    throw err;
  }

  findOrCreateCollectionsByEdgeDefinitions(g.edgeDefinitions, true);
  return new Graph(g);
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
// / @brief was docuBlock JSF_general_graph_create
// //////////////////////////////////////////////////////////////////////////////

exports._create = function (graphName, edgeDefinitions, orphanCollections, options) {
  if (!Array.isArray(orphanCollections)) {
    orphanCollections = [];
  }
  options = options || {};
  let gdb = getGraphCollection();
  let graphAlreadyExists = true;
  if (!graphName) {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_NAME.message;
    throw err;
  }
  edgeDefinitions = edgeDefinitions || [];
  if (!Array.isArray(edgeDefinitions)) {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.message;
    throw err;
  }
  // check, if a collection is already used in a different edgeDefinition
  let tmpCollections = [];
  let tmpEdgeDefinitions = {};
  edgeDefinitions.forEach(
    (edgeDefinition) => {
      let col = edgeDefinition.collection;
      if (tmpCollections.indexOf(col) !== -1) {
        let err = new ArangoError();
        err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.code;
        err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.message;
        throw err;
      }
      tmpCollections.push(col);
      tmpEdgeDefinitions[col] = edgeDefinition;
    }
  );
  gdb.toArray().forEach(
    (singleGraph) => {
      var sGEDs = singleGraph.edgeDefinitions;
      if (!Array.isArray(sGEDs)) {
        return;
      }
      sGEDs.forEach(
        (sGED) => {
          var col = sGED.collection;
          if (tmpCollections.indexOf(col) !== -1) {
            if (JSON.stringify(sGED) !== JSON.stringify(tmpEdgeDefinitions[col])) {
              let err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code;
              err.errorMessage = col + ' ' +
                                 arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message;
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
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_DUPLICATE.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_DUPLICATE.message;
    throw err;
  }

  db._flushCache();
  let collections = findOrCreateCollectionsByEdgeDefinitions(edgeDefinitions, false, options);
  orphanCollections.forEach(
    (oC) => {
      findOrCreateCollectionByName(oC, ArangoCollection.TYPE_DOCUMENT, false, options);
    }
  );

  edgeDefinitions.forEach(
    (eD, index) => {
      var tmp = sortEdgeDefinition(eD);
      edgeDefinitions[index] = tmp;
    }
  );
  orphanCollections = orphanCollections.sort();

  var data = gdb.save({
    'orphanCollections': orphanCollections,
    'edgeDefinitions': edgeDefinitions,
    '_key': graphName,
    'numberOfShards': options.numberOfShards || 1,
    'replicationFactor': options.replicationFactor || 1
  }, options);
  data.orphanCollections = orphanCollections;
  data.edgeDefinitions = edgeDefinitions;
  return new Graph(data);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_general_graph_drop
// //////////////////////////////////////////////////////////////////////////////

exports._drop = function (graphId, dropCollections) {
  let gdb = getGraphCollection();
  let graphs;

  if (!gdb.exists(graphId)) {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message;
    throw err;
  }

  checkRWPermission("_graphs");
  var graph = gdb.document(graphId);
  if (dropCollections === true) {
    graphs = exports._listObjects();
    // Here we collect all collections
    // that are leading for distribution
    let leaderCollections = new Set();
    let followerCollections = new Set();
    let dropColCB = (name) => {
      if (checkIfMayBeDropped(name, graph._key, graphs)) {
        checkRWPermission(name);
        let colObj = db[name];
        if (colObj !== undefined) {
          // If it is undefined the collection is gone already
          if (colObj.properties().distributeShardsLike === undefined) {
            leaderCollections.add(name);
          } else {
            followerCollections.add(name);
          }
        }
      }
    };
    // drop orphans
    if (!graph.orphanCollections) {
      graph.orphanCollections = [];
    }
    graph.orphanCollections.forEach(dropColCB);
    var edgeDefinitions = graph.edgeDefinitions;
    edgeDefinitions.forEach(
      function (edgeDefinition) {
        var from = edgeDefinition.from;
        var to = edgeDefinition.to;
        var collection = edgeDefinition.collection;
        dropColCB(edgeDefinition.collection);
        from.forEach(dropColCB);
        to.forEach(dropColCB);
      }
    );
    let dropColl = (c) => {
      try {
        db._drop(c);
      } catch (e) {
        internal.print("Failed to Drop: '" + c + "' reason: " + e.message);
      }
    };
    followerCollections.forEach(dropColl);
    leaderCollections.forEach(dropColl);
  }

  gdb.remove(graphId);
  db._flushCache();

  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief check if a graph exists.
// //////////////////////////////////////////////////////////////////////////////

exports._exists = function (graphId) {
  var gCol = getGraphCollection();
  return gCol.exists(graphId);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief rename a collection inside the _graphs collections
// //////////////////////////////////////////////////////////////////////////////

exports._renameCollection = function (oldName, newName) {
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
        if (doc.isSmart) {
          return;
        }
        let c = Object.assign({}, doc);
        let changed = false;
        if (c.edgeDefinitions) {
          for (let i = 0; i < c.edgeDefinitions.length; ++i) {
            var def = c.edgeDefinitions[i];
            if (def.collection === params.oldName) {
              c.edgeDefinitions[i].collection = params.newName;
              changed = true;
            }
            for (let j = 0; j < def.from.length; ++j) {
              if (def.from[j] === params.oldName) {
                c.edgeDefinitions[i].from[j] = params.newName;
                changed = true;
              }
            }
            for (let j = 0; j < def.to.length; ++j) {
              if (def.to[j] === params.oldName) {
                c.edgeDefinitions[i].to[j] = params.newName;
                changed = true;
              }
            }
          }
        }
        for (let i = 0; i < c.orphanCollections.length; ++i) {
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
// / @brief was docuBlock JSF_general_graph_list
// //////////////////////////////////////////////////////////////////////////////

exports._list = function () {
  return db._query(`FOR x IN _graphs RETURN x._key`).toArray();
};

exports._listObjects = function () {
  return db._query(`FOR x IN _graphs RETURN x`).toArray();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Compatibility functions for 2.8
// /        This function registeres user-defined functions that follow the
// /        same API as the former GRAPH_* functions did.
// /        Most of these AQL functions can be simply replaced by calls to these.
// //////////////////////////////////////////////////////////////////////////////

exports._registerCompatibilityFunctions = function () {
  const aqlfunctions = require('@arangodb/aql/functions');
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

exports.__GraphClass = Graph;

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
