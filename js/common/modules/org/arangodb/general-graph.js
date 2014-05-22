/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Graph, arguments */

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
/// @author Florian Bartels
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


var arangodb = require("org/arangodb"),
  ArangoCollection = arangodb.ArangoCollection,
  ArangoError = arangodb.ArangoError,
  db = arangodb.db,
  errors = arangodb.errors,
  _ = require("underscore");


// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/general-graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief transform a string into an array.
////////////////////////////////////////////////////////////////////////////////


var stringToArray = function (x) {
  if (typeof x === "string") {
    return [x];
  }
  return _.clone(x);
};

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
  var col = db._collection(name),res = false;
  if (col === null && !noCreate) {
    if (type === ArangoCollection.TYPE_DOCUMENT) {
      col = db._create(name);
    } else {
      col = db._createEdgeCollection(name);
    }
    res = true;
  } else if (!(col instanceof ArangoCollection) || col.type() !== type) {
    throw "<" + name + "> must be a " +
    (type === ArangoCollection.TYPE_DOCUMENT ? "document" : "edge")
    + " collection";
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

var _getGraphCollection = function() {
  var gCol = db._graphs;
  if (gCol === null || gCol === undefined) {
    throw "_graphs collection does not exist.";
  }
  return gCol;
};



// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/general-graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                             Fluent AQL Interface
// -----------------------------------------------------------------------------

var AQLStatement = function(query, isEdgeQuery) {
  this.query = query;
  this.edgeQuery = isEdgeQuery || false;
};

AQLStatement.prototype.printQuery = function() {
  return this.query;
};

AQLStatement.prototype.isEdgeQuery = function() {
  return this.edgeQuery;
};

// -----------------------------------------------------------------------------
// --SECTION--                             AQL Generator
// -----------------------------------------------------------------------------

var AQLGenerator = function(graph) {
  this.stack = [];
  this.bindVars = {
    "graphName": graph.__name
  };
  this.graph = graph;
  this.lastEdgeVar = "";
};

AQLGenerator.prototype.edges = function(startVertex, direction) {
  var edgeName = "edges_" + this.stack.length;
  var query = "FOR " + edgeName
    + " IN GRAPH_EDGES(@graphName,@startVertex_"
    + this.stack.length + ',"'
    + direction + '")';
  this.bindVars["startVertex_" + this.stack.length] = startVertex;
  var stmt = new AQLStatement(query, true);
  this.stack.push(stmt);
  this.lastEdgeVar = edgeName;
  return this;
};


AQLGenerator.prototype.neighbors = function(startVertex, direction) {
  //var edgeName = "edges_" + this.stack.length;
  var query = "FOR e"
    + " IN GRAPH_NEIGHBORS(@graphName,@startVertex_"
    + this.stack.length + ',"'
    + direction + '")';
  this.bindVars["startVertex_" + this.stack.length] = startVertex;
  var stmt = new AQLStatement(query, true);
  this.stack.push(stmt);
  this.lastEdgeVar = "e";
  return this;
};


AQLGenerator.prototype.getLastEdgeVar = function() {
  if (this.lastEdgeVar === "") {
    return false;
  }
  return this.lastEdgeVar;
};

AQLGenerator.prototype.restrict = function(restrictions) {
  var rest = stringToArray(restrictions);
  var unknown = [];
  var g = this.graph;
  _.each(rest, function(r) {
    if (!g.__edgeCollections[r]) {
      unknown.push(r);
    }
  });
  if (unknown.length > 0) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "edge collections: " + unknown.join(" and ") + " are not known to the graph";
    throw err;
  }
  var lastQuery = this.stack.pop();
  if (!lastQuery.isEdgeQuery()) {
    this.stack.push(lastQuery);
    throw "Restrict can only be applied directly after edge selectors";
  }
  lastQuery.query = lastQuery.query.replace(")", ",{},@restrictions_" + this.stack.length + ")");
  lastQuery.edgeQuery = false;
  this.bindVars["restrictions_" + this.stack.length] = rest;
  this.stack.push(lastQuery);
  return this;
};

AQLGenerator.prototype.filter = function(example) {
  var ex = [];
  if (Object.prototype.toString.call(example) !== "[object Array]") {
    if (Object.prototype.toString.call(example) !== "[object Object]") {
      throw "The example has to be an Object, or an Array";
    }
    ex = [example];
  } else {
    ex = example;
  }
  var query = "FILTER MATCHES(" + this.getLastEdgeVar() + "," + JSON.stringify(ex) + ")";
  this.stack.push(new AQLStatement(query));
  return this;
};

/* Old Filter version, string replacements broxen
AQLGenerator.prototype.filter = function(condition) {
  var con = condition.replace("e.", this.getLastEdgeVar() + "."); 
  var query = "FILTER " + con;
  this.stack.push(new AQLStatement(query));
  return this;
};
*/

/* Old LET version, string replacements broxen
AQLGenerator.prototype.let = function(assignment) {
  var asg = assignment.replace("e.", this.getLastEdgeVar() + "."); 
  var query = "LET " + asg;
  this.stack.push(new AQLStatement(query));
  return this;
};
*/

AQLGenerator.prototype.printQuery = function() {
  return this.stack.map(function(stmt) {
    return stmt.printQuery();
  }).join(" ");
};

AQLGenerator.prototype.execute = function() {
  var query = this.printQuery();
  var bindVars = this.bindVars;
  query += " RETURN " + this.getLastEdgeVar();
  return db._query(query, bindVars);
};

AQLGenerator.prototype.toArray = function() {
  return this.execute().toArray();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief define an undirected relation.
////////////////////////////////////////////////////////////////////////////////


var _undirectedRelationDefinition = function (relationName, vertexCollections) {

  if (arguments.length < 2) {
    throw "method _undirectedRelationDefinition expects 2 arguments";
  }

  if (typeof relationName !== "string" || relationName === "") {
    throw "<relationName> must be a not empty string";
  }

  if (!isValidCollectionsParameter(vertexCollections)) {
    throw "<vertexCollections> must be a not empty string or array";
  }

  return {
    collection: relationName,
    from: stringToArray(vertexCollections),
    to: stringToArray(vertexCollections)
  };
};


////////////////////////////////////////////////////////////////////////////////
/// @brief define an directed relation.
////////////////////////////////////////////////////////////////////////////////


var _directedRelationDefinition = function (
  relationName, fromVertexCollections, toVertexCollections) {

  if (arguments.length < 3) {
    throw "method _directedRelationDefinition expects 3 arguments";
  }

  if (typeof relationName !== "string" || relationName === "") {
    throw "<relationName> must be a not empty string";
  }

  if (!isValidCollectionsParameter(fromVertexCollections)) {
    throw "<fromVertexCollections> must be a not empty string or array";
  }

  if (!isValidCollectionsParameter(toVertexCollections)) {
    throw "<toVertexCollections> must be a not empty string or array";
  }

  return {
    collection: relationName,
    from: stringToArray(fromVertexCollections),
    to: stringToArray(toVertexCollections)
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list of edge definitions
////////////////////////////////////////////////////////////////////////////////


var _edgeDefinitions = function () {

  var res = [], args = arguments;
  Object.keys(args).forEach(function (x) {
   res.push(args[x]);
  });

  return res;

};


////////////////////////////////////////////////////////////////////////////////
/// @brief extend a list of edge definitions
////////////////////////////////////////////////////////////////////////////////


var _extendEdgeDefinitions = function (edgeDefinition) {

  var args = arguments, i = 0;

  Object.keys(args).forEach(function (x) {
    i++;
    if (i == 1) {return;}
    edgeDefinition.push(args[x]);
  });
};



////////////////////////////////////////////////////////////////////////////////
/// @brief create a new graph
////////////////////////////////////////////////////////////////////////////////


var _create = function (graphName, edgeDefinitions) {

  var gdb = _getGraphCollection(),
    g,
    graphAlreadyExists = true,
    collections;

  if (!graphName) {
    throw "a graph name is required to create a graph.";
  }
  if (!Array.isArray(edgeDefinitions) || edgeDefinitions.length === 0) {
    throw "at least one edge definition is required to create a graph.";
  }
  try {
    g = gdb.document(graphName);
  } catch (e) {
    if (e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw e;
    }
    graphAlreadyExists = false;
  }

  if (graphAlreadyExists) {
    throw "graph " + graphName + " already exists.";
  }

  collections = findOrCreateCollectionsByEdgeDefinitions(edgeDefinitions, false);

  gdb.save({
    'edgeDefinitions' : edgeDefinitions,
    '_key' : graphName
  });

  return new Graph(graphName, edgeDefinitions, collections[0], collections[1]);

};


////////////////////////////////////////////////////////////////////////////////
/// @brief constructor.
////////////////////////////////////////////////////////////////////////////////

var Graph = function(graphName, edgeDefinitions, vertexCollections, edgeCollections) {
  var self = this;
  this.__name = graphName;
  this.__vertexCollections = vertexCollections;
  this.__edgeCollections = edgeCollections;
  this.__edgeDefinitions = edgeDefinitions;


  _.each(vertexCollections, function(obj, key) {
    self[key] = obj;
    var old_remove = obj.remove.bind(obj);
    obj.remove = function(vertexId, options) {
      var myEdges = self._EDGES(vertexId);
      myEdges.forEach(
        function(edgeObj) {
          var edgeId = edgeObj._id;
          var edgeCollection = edgeId.split("/")[0];
          if (db[edgeCollection] && db[edgeCollection].exists(edgeId)) {
            db[edgeCollection].remove(edgeId);
          }
        }
      );

      if (options === null || options === undefined) {
        return old_remove(vertexId);
      }
      return old_remove(vertexId, options);
    };
  });

  _.each(edgeCollections, function(obj, key) {
    self[key] = obj;
    var old_save = obj.save.bind(obj);
    obj.save = function(from, to, data) {
      edgeDefinitions.forEach(
        function(edgeDefinition) {
          if (edgeDefinition.collection === key) {
            var fromCollection = from.split("/")[0];
            var toCollection = to.split("/")[0];
            if (! _.contains(edgeDefinition.from, fromCollection)
              || ! _.contains(edgeDefinition.to, toCollection)) {
              throw "Edge is not allowed between " + from + " and " + to + ".";
            }
          }
        }
      );
      return old_save(from, to, data);
    };
  });
};



////////////////////////////////////////////////////////////////////////////////
/// @brief load a graph.
////////////////////////////////////////////////////////////////////////////////

var _graph = function(graphName) {

  var gdb = _getGraphCollection(),
    g, collections;

  try {
    g = gdb.document(graphName);
  } 
  catch (e) {
    if (e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw e;
    }
    throw "graph " + graphName + " does not exists.";
  }

  collections = findOrCreateCollectionsByEdgeDefinitions(g.edgeDefinitions, true);

  return new Graph(graphName, g.edgeDefinitions, collections[0], collections[1]);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a graph exists.
////////////////////////////////////////////////////////////////////////////////

var _exists = function(graphId) {
  var gCol = _getGraphCollection();
  return gCol.exists(graphId);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drop a graph.
////////////////////////////////////////////////////////////////////////////////

var _drop = function(graphId, dropCollections) {

  var gdb = _getGraphCollection();

  if (!gdb.exists(graphId)) {
    throw "Graph " + graphId + " does not exist.";
  }

  if (dropCollections !== false) {
    var graph = gdb.document(graphId);
    var edgeDefinitions = graph.edgeDefinitions;
    edgeDefinitions.forEach(
      function(edgeDefinition) {
        var from = edgeDefinition.from;
        var to = edgeDefinition.to;
        var edge = edgeDefinition.collection;
        db._drop(edge);
        from.forEach(
          function(col) {
            db._drop(col);
          }
        );
        to.forEach(
          function(col) {
            db._drop(col);
          }
        );
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
  return _.values(this.__vertexCollections);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _EDGES(vertexId).
////////////////////////////////////////////////////////////////////////////////

// might be needed from AQL itself
Graph.prototype._EDGES = function(vertexId) {
  var edgeCollections = this._edgeCollections();
  var result = [];


  edgeCollections.forEach(
    function(edgeCollection) {
      //todo: test, if collection may point to vertex
      result = result.concat(edgeCollection.edges(vertexId));
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief INEDGES(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._INEDGES = function(vertexId) {
  var edgeCollections = this._edgeCollections();
  var result = [];


  edgeCollections.forEach(
    function(edgeCollection) {
      //todo: test, if collection may point to vertex
      result = result.concat(edgeCollection.inEdges(vertexId));
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._OUTEDGES = function(vertexId) {
  var edgeCollections = this._edgeCollections();
  var result = [];


  edgeCollections.forEach(
    function(edgeCollection) {
      //todo: test, if collection may point to vertex
      result = result.concat(edgeCollection.outEdges(vertexId));
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _edges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._edges = function(vertexId) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.edges(vertexId, "any");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _inEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._inEdges = function(vertexId) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.edges(vertexId, "inbound");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _outEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._outEdges = function(vertexId) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.edges(vertexId, "outbound");
};
////////////////////////////////////////////////////////////////////////////////
/// @brief get ingoing vertex of an edge.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getInVertex = function(edgeId) {
  var edgeCollection = this._getEdgeCollectionByName(edgeId.split("/")[0]);
  var document = edgeCollection.document(edgeId);
  if (document) {
    var vertexId = document._from;
    var vertexCollection = this._getVertexCollectionByName(vertexId.split("/")[0]);
    return vertexCollection.document(vertexId);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get outgoing vertex of an edge .
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getOutVertex = function(edgeId) {
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
  throw "Collection " + name + " does not exist in graph.";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get vertex collection by name.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getVertexCollectionByName = function(name) {
  if (this.__vertexCollections[name]) {
    return this.__vertexCollections[name];
  }
  throw "Collection " + name + " does not exist in graph.";
};


////////////////////////////////////////////////////////////////////////////////
/// @brief get neighbors of a vertex in the graph.
////////////////////////////////////////////////////////////////////////////////
/*
direction:
  "inbound" -> consider only inbound edges
"outbount" -> consider only outbound edges
"any"(default) -> consider both directions
weight: attribute-name -> use this attribute to determine edgeweight
weight_function: function -> use this function to calculate the weight
default-weight -> us this value if weight could not be calculated otherwise, default is Infinity
only: function -> will be invoked on any edge, neighbors will only be included if this returns true or is not defined.*/

Graph.prototype._neighbors = function(vertex, options) {
  var current_vertex,
    target_array = [],
    addNeighborToList,
    AQLStmt;

  if (! options) {
    options = { };
  }

  var direction = options.direction || 'any',
    weight = options.weight,
    weight_function = options.weight_function,
    default_weight = options.default_weight || Infinity,
    only = options.only;

  addNeighborToList = function (current_edge, current_vertex) {
    var neighbor_info;

    neighbor_info = { id: current_vertex._id };
    if (weight !== undefined) {
      neighbor_info.weight = current_edge[weight] || default_weight;
    } else if (weight_function !== undefined) {
      neighbor_info.weight = weight_function(current_edge);
    } else {
      neighbor_info.weight = 1;
    }

    if ((only === undefined) || (only(current_edge))) {
      target_array.push(neighbor_info);
      }
  };

  AQLStmt = new AQLGenerator(this);
  //require("internal").print(AQLStmt.neighbors(vertex, direction).toArray())
  AQLStmt.neighbors(vertex, direction).toArray().forEach(function (n) {
    addNeighborToList(n.edge, n.vertex);
  });

  return target_array;
};



////////////////////////////////////////////////////////////////////////////////
/// @brief get common neighbors of two vertices in the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._listCommonNeighbors = function(vertex1, vertex2, options) {

};

////////////////////////////////////////////////////////////////////////////////
/// @brief get amount of common neighbors of two vertices in the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._amountCommonNeighbors = function(vertex1, vertex2, options) {

};

////////////////////////////////////////////////////////////////////////////////
/// @brief get common properties of two vertices in the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._listCommonProperties = function(vertex1, vertex2, options) {

};

////////////////////////////////////////////////////////////////////////////////
/// @brief get amount of common properties of two vertices in the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._amountCommonProperties = function(vertex1, vertex2, options) {

}

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports._undirectedRelationDefinition = _undirectedRelationDefinition;
exports._directedRelationDefinition = _directedRelationDefinition;
exports._graph = _graph;
exports._edgeDefinitions = _edgeDefinitions;
exports._extendEdgeDefinitions = _extendEdgeDefinitions;
exports._create = _create;
exports._drop = _drop;
exports._exists = _exists;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
