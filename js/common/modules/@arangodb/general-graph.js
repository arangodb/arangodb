/*jshint strict: false */
/*global ArangoClusterComm */

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

var arangodb = require("@arangodb"),
  internal = require("internal"),
  ArangoCollection = arangodb.ArangoCollection,
  ArangoError = arangodb.ArangoError,
  db = arangodb.db,
  errors = arangodb.errors,
  _ = require("lodash");



////////////////////////////////////////////////////////////////////////////////
/// @brief transform a string into an array.
////////////////////////////////////////////////////////////////////////////////

var stringToArray = function (x) {
  if (typeof x === "string") {
    return [x];
  }
  return x.slice();
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
  var col = db._collection(name),
    res = false;
  if (col === null && ! noCreate) {
    if (type === ArangoCollection.TYPE_DOCUMENT) {
      col = db._create(name);
    } else {
      col = db._createEdgeCollection(name);
    }
    res = true;
  } 
  else if (! (col instanceof ArangoCollection)) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION.code;
    err.errorMessage = name + arangodb.errors.ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION.message;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_example_description
////////////////////////////////////////////////////////////////////////////////

var transformExample = function(example) {
  if (example === undefined) {
    return {};
  }
  if (typeof example === "string") {
    return {_id: example};
  }
  if (typeof example === "object") {
    if (Array.isArray(example)) {
      return _.map(example, function(e) {
        if (typeof e === "string") {
          return {_id: e};
        }
        return e;
      });
    }
    return example;
  }
  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING.code;
  err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING.message;
  throw err;
};

var checkAllowsRestriction = function(list, rest, msg) {
  var unknown = [];
  var colList = _.map(list, function(item) {
    return item.name();
  });
  _.each(rest, function(r) {
    if (!_.contains(colList, r)) {
      unknown.push(r);
    }
  });
  if (unknown.length > 0) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = msg + ": "
    + unknown.join(" and ")
    + " are not known to the graph";
    throw err;
  }
  return true;
};




var AQLStatement = function(query, type) {
  this.query = query;
  if (type) {
    this.type = type;
  }
};

AQLStatement.prototype.printQuery = function() {
  return this.query;
};

AQLStatement.prototype.isPathQuery = function() {
  return this.type === "path";
};

AQLStatement.prototype.isPathVerticesQuery = function() {
  return this.type === "pathVertices";
};

AQLStatement.prototype.isPathEdgesQuery = function() {
  return this.type === "pathEdges";
};

AQLStatement.prototype.isEdgeQuery = function() {
  return this.type === "edge";
};

AQLStatement.prototype.isVertexQuery = function() {
  return this.type === "vertex";
};

AQLStatement.prototype.isNeighborQuery = function() {
  return this.type === "neighbor";
};

AQLStatement.prototype.allowsRestrict = function() {
  return this.isEdgeQuery()
    || this.isVertexQuery()
    || this.isNeighborQuery();
};


////////////////////////////////////////////////////////////////////////////////
/// @brief Starting point of the fluent interface.
///
/// Only for internal use.
////////////////////////////////////////////////////////////////////////////////

var AQLGenerator = function(graph) {
  this.stack = [];
  this.callStack = [];
  this.bindVars = {
    "graphName": graph.__name
  };
  this.graph = graph;
  this.cursor = null;
  this.lastVar = "";
  this._path = [];
  this._pathVertices = [];
  this._pathEdges = [];
  this._getPath = false;
};

AQLGenerator.prototype._addToPrint = function(name) {
  var args = Array.prototype.slice.call(arguments);
  args.shift(); // The Name
  var stackEntry = {};
  stackEntry.name = name;
  if (args.length > 0 && args[0] !== undefined) {
    stackEntry.params = args;
  } else {
    stackEntry.params = [];
  }
  this.callStack.push(stackEntry);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Print the call stack of this query
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._PRINT = function(context) {
  context.output = "[ GraphAQL ";
  context.output += this.graph.__name;
  _.each(this.callStack, function(call) {
    if(context.prettyPrint) {
      context.output += "\n";
    }
    context.output += ".";
    context.output += call.name;
    context.output += "(";
    var i = 0;
    for(i = 0; i < call.params.length; ++i) {
      if (i > 0) {
        context.output += ", ";
      }
      internal.printRecursive(call.params[i], context);
    }
    context.output += ")";
  });
  context.output += " ] ";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Dispose and reset the current cursor of the query
///
/// Only for internal use.
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._clearCursor = function() {
  if (this.cursor) {
    this.cursor.dispose();
    this.cursor = null;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute the query and keep the cursor
///
/// Only for internal use.
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._createCursor = function() {
  if (!this.cursor) {
    this.cursor = this.execute();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief General edge query, takes direction as parameter
///
/// This will create the general AQL statement to load edges
/// connected to the vertices selected in the step before.
/// Will also bind the options into bindVars.
///
/// Only for internal use, user gets different functions for directions
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._edges = function(edgeExample, options) {
  this._clearCursor();
  this.options = options || {};
  var ex = transformExample(edgeExample);
  var edgeName = "edges_" + this.stack.length;
  var query = "FOR " + edgeName
    + ' IN GRAPH_EDGES(@graphName';
  if (!this.getLastVar()) {
    query += ',{}';
  } else {
    query += ',' + this.getLastVar();
  }
  query += ',@options_'
    + this.stack.length + ')';
  if (!Array.isArray(ex)) {
    ex = [ex];
  }
  this.options.edgeExamples = ex;
  this.options.includeData = true;
  this.bindVars["options_" + this.stack.length] = this.options;
  var stmt = new AQLStatement(query, "edge");
  this.stack.push(stmt);
  this.lastVar = edgeName;
  this._path.push(edgeName);
  this._pathEdges.push(edgeName);
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_edges
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.edges = function(example) {
  this._addToPrint("edges", example);
  return this._edges(example, {direction: "any"});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_outEdges
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.outEdges = function(example) {
  this._addToPrint("outEdges", example);
  return this._edges(example, {direction: "outbound"});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_inEdges
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.inEdges = function(example) {
  this._addToPrint("inEdges", example);
  return this._edges(example, {direction: "inbound"});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief General vertex query, takes direction as parameter
///
/// This will create the general AQL statement to load vertices
/// connected to the edges selected in the step before.
/// Will also bind the options into bindVars.
///
/// Only for internal use, user gets different functions for directions
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._vertices = function(example, options, mergeWith) {
  this._clearCursor();
  this.options = options || {};
  var ex = transformExample(example);
  var vertexName = "vertices_" + this.stack.length;
  var query = "FOR " + vertexName
    + " IN GRAPH_VERTICES(@graphName,";
  if (mergeWith !== undefined) {
    if (Array.isArray(mergeWith)) {
      var i;
      query += "[";
      for (i = 0; i < mergeWith.length; ++i) {
        if (i > 0) {
          query += ",";
        }
        query += "MERGE(@vertexExample_" + this.stack.length 
          + "," + mergeWith[i] + ")";
      }
      query += "]";
    } else {
      if (Array.isArray(ex)) {
        query += "@vertexExample_" + this.stack.length + " [ * RETURN MERGE(CURRENT," + mergeWith + ")]";
      }
      else {
        query += "MERGE(@vertexExample_" + this.stack.length 
          + "," + mergeWith + ")";
      }
    }
  } else {
    query += "@vertexExample_" + this.stack.length;
  }
  query += ',@options_' + this.stack.length + ')';
  this.bindVars["vertexExample_" + this.stack.length] = ex;
  this.bindVars["options_" + this.stack.length] = this.options;
  var stmt = new AQLStatement(query, "vertex");
  this.stack.push(stmt);
  this.lastVar = vertexName;
  this._path.push(vertexName);
  this._pathVertices.push(vertexName);
  return this;

};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_vertices
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.vertices = function(example) {
  this._addToPrint("vertices", example);
  if (!this.getLastVar()) {
    return this._vertices(example);
  }
  var edgeVar = this.getLastVar();
  return this._vertices(example, undefined,
    ["{'_id': " + edgeVar + "._from}", "{'_id': " + edgeVar + "._to}"]);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_fromVertices
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.fromVertices = function(example) {
  this._addToPrint("fromVertices", example);
  if (!this.getLastVar()) {
    return this._vertices(example);
  }
  var edgeVar = this.getLastVar();
  return this._vertices(example, undefined, "{'_id': " + edgeVar + "._from}");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_toVertices
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.toVertices = function(example) {
  this._addToPrint("toVertices", example);
  if (!this.getLastVar()) {
    return this._vertices(example);
  }
  var edgeVar = this.getLastVar();
  return this._vertices(example, undefined, "{'_id': " + edgeVar + "._to}");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the variable holding the last result
///
/// Only for internal use.
/// The return statement of the AQL query has to return
/// this value.
/// Also chaining has to use this variable to restrict
/// queries in the next step to only values from this set.
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.getLastVar = function() {
  if (this.lastVar === "") {
    return false;
  }
  return this.lastVar;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_path
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.path = function() {
  this._clearCursor();
  var statement = new AQLStatement("", "path");
  this.stack.push(statement);
  return this;
};

AQLGenerator.prototype.pathVertices = function() {
  this._clearCursor();
  var statement = new AQLStatement("", "pathVertices");
  this.stack.push(statement);
  return this;
};

AQLGenerator.prototype.pathEdges = function() {
  this._clearCursor();
  var statement = new AQLStatement("", "pathEdges");
  this.stack.push(statement);
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_neighbors
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.neighbors = function(vertexExample, options) {
  this._addToPrint("neighbors", vertexExample, options);
  var ex = transformExample(vertexExample);
  var resultName = "neighbors_" + this.stack.length;
  var query = "FOR " + resultName
    + " IN GRAPH_NEIGHBORS(@graphName,"
    + this.getLastVar()
    + ',@options_'
    + this.stack.length + ')';
  var opts;
  if (options) {
    opts = _.extend({}, options);
  } else {
    opts = {};
  }
  opts.neighborExamples = ex;
  opts.includeData = true;
  this.bindVars["options_" + this.stack.length] = opts;
  var stmt = new AQLStatement(query, "neighbor");
  this.stack.push(stmt);

  this.lastVar = resultName;
  this._path.push(resultName);
  this._pathVertices.push(resultName);

  /*
  this.lastVar = resultName + ".vertex";
  this._path.push(resultName + ".path");
  this._pathVertices.push("SLICE(" + resultName + ".path.vertices, 1)");
  this._pathEdges.push(resultName + ".path.edges");
  */
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the last statement that can be restricted to collections
///
/// Only for internal use.
/// This returnes the last statement that can be restricted to
/// specific collections.
/// Required to allow a chaining of `restrict` after `filter` for instance.
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._getLastRestrictableStatementInfo = function() {
  var i = this.stack.length - 1;
  while (!this.stack[i].allowsRestrict()) {
    i--;
  }
  return {
    statement: this.stack[i],
    options: this.bindVars["options_" + i]
  };
};


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_restrict
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.restrict = function(restrictions) {
  var rest = stringToArray(restrictions);
  if (rest.length === 0) {
    return this;
  }
  this._addToPrint("restrict", restrictions);
  this._clearCursor();
  var lastQueryInfo = this._getLastRestrictableStatementInfo();
  var lastQuery = lastQueryInfo.statement;
  var opts = lastQueryInfo.options;
  var restricts;
  if (lastQuery.isEdgeQuery()) {
    checkAllowsRestriction(
      this.graph._edgeCollections(),
      rest,
      "edge collections"
    );
    restricts = opts.edgeCollectionRestriction || [];
    opts.edgeCollectionRestriction = restricts.concat(restrictions);
  } else if (lastQuery.isVertexQuery() || lastQuery.isNeighborQuery()) {
    checkAllowsRestriction(
      this.graph._vertexCollections(),
      rest,
      "vertex collections"
    );
    restricts = opts.vertexCollectionRestriction || [];
    opts.vertexCollectionRestriction = restricts.concat(restrictions);
  }
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_filter
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.filter = function(example) {
  this._addToPrint("filter", example);
  this._clearCursor();
  var ex = [];
  if (Object.prototype.toString.call(example) !== "[object Array]") {
    if (Object.prototype.toString.call(example) !== "[object Object]") {
      var err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT.code;
      err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT.message;
      throw err;
    }
    ex = [example];
  } else {
    ex = example;
  }
  var query = "FILTER MATCHES(" + this.getLastVar() + "," + JSON.stringify(ex) + ")";
  this.stack.push(new AQLStatement(query));
  return this;
};

AQLGenerator.prototype.printQuery = function() {
  return this.stack.map(function(stmt) {
    return stmt.printQuery();
  }).join(" ");
};

AQLGenerator.prototype.execute = function() {
  this._clearCursor();
  var query = this.printQuery();
  var bindVars = this.bindVars;
  if (this.stack[this.stack.length-1].isPathQuery()) {
    query += " RETURN [" + this._path + "]";
  } else if (this.stack[this.stack.length-1].isPathVerticesQuery()) {
    query += " RETURN FLATTEN([" + this._pathVertices + "])";
  } else if (this.stack[this.stack.length-1].isPathEdgesQuery()) {
    query += " RETURN FLATTEN([" + this._pathEdges + "])";
  } else {
    query += " RETURN " + this.getLastVar();
  }
  return db._query(query, bindVars, {count: true});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_toArray
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.toArray = function() {
  this._createCursor();

  return this.cursor.toArray();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_count
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.count = function() {
  this._createCursor();
  return this.cursor.count();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_hasNext
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.hasNext = function() {
  this._createCursor();
  return this.cursor.hasNext();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fluent_aql_next
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.next = function() {
  this._createCursor();
  return this.cursor.next();
};


////////////////////////////////////////////////////////////////////////////////
/// Deprecated block
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_undirectedRelation
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
/// @brief was docuBlock JSF_general_graph_directedRelation
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_relation
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
/// @brief was docuBlock JSF_general_graph_list
////////////////////////////////////////////////////////////////////////////////

var _list = function() {
  var gdb = getGraphCollection();
  return _.pluck(gdb.toArray(), "_key");
};


var _listObjects = function() {
  return getGraphCollection().toArray();
};




////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_edge_definitions
////////////////////////////////////////////////////////////////////////////////


var _edgeDefinitions = function () {

  var res = [], args = arguments;
  Object.keys(args).forEach(function (x) {
   res.push(args[x]);
  });

  return res;

};


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_extend_edge_definitions
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
/// @brief was docuBlock JSF_general_graph_how_to_create
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_create
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
/// @brief was docuBlock JSF_general_graph_vertex_collection_save
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_vertex_collection_replace
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_vertex_collection_update
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_vertex_collection_remove
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_edge_collection_save
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_edge_collection_replace
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_edge_collection_update
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_edge_collection_remove
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
  if (useBuiltIn && require("@arangodb/cluster").isCoordinator()) {
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
/// @brief was docuBlock JSF_general_graph_graph
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
    g._rev, g._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a graph exists.
////////////////////////////////////////////////////////////////////////////////

var _exists = function(graphId) {
  var gCol = getGraphCollection();
  return gCol.exists(graphId);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief rename a collection inside the _graphs collections
////////////////////////////////////////////////////////////////////////////////

var _renameCollection = function(oldName, newName) {
  db._executeTransaction({
    collections: {
      write: "_graphs"
    },
    action: function(params) {
      var gdb = getGraphCollection();
      if (! gdb) {
        return;
      }
      gdb.toArray().forEach(function(doc) {
        var c = _.extend({}, doc), i, j, changed = false;
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
/// @brief was docuBlock JSF_general_graph_drop
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
/// @brief was docuBlock JSF_general_graph_edges
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._edges = function(edgeExample) {
  var AQLStmt = new AQLGenerator(this);
  // If no direction is specified all edges are duplicated.
  // => For initial requests a direction has to be set
  return AQLStmt.outEdges(edgeExample);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_vertices
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._vertices = function(example) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.vertices(example);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_fromVertex
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
/// @brief was docuBlock JSF_general_graph_toVertex
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
  err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.code;
  err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.message + ": " + name;
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
/// @brief was docuBlock JSF_general_graph_neighbors
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
/// @brief was docuBlock JSF_general_graph_common_neighbors
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._commonNeighbors = function(vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {

  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_NEIGHBORS(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options1'
    + ',@options2'
    + ') RETURN e';
  optionsVertex1 = optionsVertex1 || {};
  optionsVertex2 = optionsVertex2 || {};
  var bindVars = {
    "graphName": this.__name,
    "options1": optionsVertex1,
    "options2": optionsVertex2,
    "ex1": ex1,
    "ex2": ex2
  };
  return db._query(query, bindVars, {count: true}).toArray();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_count_common_neighbors
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._countCommonNeighbors = function(vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {
  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_NEIGHBORS(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options1'
    + ',@options2'
    + ') RETURN [e.left, e.right, LENGTH(e.neighbors)]';
  optionsVertex1 = optionsVertex1 || {};
  optionsVertex2 = optionsVertex2 || {};
  var bindVars = {
    "graphName": this.__name,
    "options1": optionsVertex1,
    "options2": optionsVertex2,
    "ex1": ex1,
    "ex2": ex2
  };

  var result = db._query(query, bindVars, {count: true}).toArray(),
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
/// @brief was docuBlock JSF_general_graph_common_properties
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._commonProperties = function(vertex1Example, vertex2Example, options) {

  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_PROPERTIES(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options'
    + ')  SORT  ATTRIBUTES(e)[0] RETURN e';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  return db._query(query, bindVars, {count: true}).toArray();

};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_count_common_properties
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._countCommonProperties = function(vertex1Example, vertex2Example, options) {
  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_PROPERTIES(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options'
    + ') FOR a in ATTRIBUTES(e)  SORT  ATTRIBUTES(e)[0] RETURN [ ATTRIBUTES(e)[0], LENGTH(e[a]) ]';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(query, bindVars, {count: true}).toArray(), returnHash = [];
  result.forEach(function (r) {
    var tmp = {};
    tmp[r[0]] = r[1];
    returnHash.push(tmp);
  });
  return returnHash;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_paths
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._paths = function(options) {
  var query = "RETURN"
    + " GRAPH_PATHS(@graphName"
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
/// @brief was docuBlock JSF_general_graph_shortest_path
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._shortestPath = function(startVertexExample, endVertexExample, options) {
  var ex1 = transformExample(startVertexExample);
  var ex2 = transformExample(endVertexExample);
  var query = "RETURN"
    + " GRAPH_SHORTEST_PATH(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(query, bindVars).toArray();
  return result;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_distance_to
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._distanceTo = function(startVertexExample, endVertexExample, options) {
  var ex1 = transformExample(startVertexExample);
  var ex2 = transformExample(endVertexExample);
  var query = "RETURN"
    + " GRAPH_DISTANCE_TO(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(query, bindVars).toArray();
  return result[0];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_absolute_eccentricity
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._absoluteEccentricity = function(vertexExample, options) {
  var ex1 = transformExample(vertexExample);
  var query = "RETURN"
    + " GRAPH_ABSOLUTE_ECCENTRICITY(@graphName"
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
  if (result.length === 1) {
    return result[0];
  }
  return result;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_eccentricity
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._eccentricity = function(options) {
  var query = "RETURN"
    + " GRAPH_ECCENTRICITY(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};



////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_absolute_closeness
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
  if (result.length === 1) {
    return result[0];
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_closeness
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
  if (result.length === 1) {
    return result[0];
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_absolute_betweenness
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._absoluteBetweenness = function(example, options) {

  var query = "RETURN"
    + " GRAPH_ABSOLUTE_BETWEENNESS(@graphName"
    + ",@example"
    + ",@options"
    + ")";
  options = options || {};
  var bindVars = {
    "example": example,
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_betweenness
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
  if (result.length === 1) {
    return result[0];
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_radius
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
  if (result.length === 1) {
    return result[0];
  }
  return result;
};



////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_diameter
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
  if (result.length === 1) {
    return result[0];
  }
  return result;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph__extendEdgeDefinitions
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
  //in different graph
  db._graphs.toArray().forEach(
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
  db._graphs.update(this.__name, {edgeDefinitions: this.__edgeDefinitions});
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

  //replace edgeDefintion
  eDs.forEach(
    function(eD, id) {
      if(eD.collection === edgeDefinition.collection) {
        gotAHit = true;
        eDs[id].from = edgeDefinition.from;
        eDs[id].to = edgeDefinition.to;
        db._graphs.update(graph._key, {edgeDefinitions: eDs});
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
/// @brief was docuBlock JSF_general_graph__editEdgeDefinition
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
/// @brief was docuBlock JSF_general_graph__deleteEdgeDefinition
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

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph__addVertexCollection
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
  db._graphs.update(this.__name, {orphanCollections: this.__orphanCollections});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph__orphanCollections
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._orphanCollections = function() {
  return this.__orphanCollections;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph__removeVertexCollection
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
  db._graphs.update(this.__name, {orphanCollections: this.__orphanCollections});

  if (dropCollection === true) {
    var graphs = getGraphCollection().toArray();
    if (checkIfMayBeDropped(vertexCollectionName, null, graphs)) {
      db._drop(vertexCollectionName);
    }
  }
  updateBindCollections(this);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_connectingEdges
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getConnectingEdges = function(vertexExample1, vertexExample2, options) {
  options = options || {};

  var opts = {
    includeData: true
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

  var query = "RETURN"
    + " GRAPH_EDGES(@graphName"
    + ',@vertexExample'
    + ',@options'
    + ')';
  var bindVars = {
    "graphName": this.__name,
    "vertexExample": vertexExample1,
    "options": opts
  };
  var result = db._query(query, bindVars).toArray();
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


/// Deprecated function (announced 2.3)
exports._undirectedRelation = _undirectedRelation;
/// Deprecated function (announced 2.3)
exports._directedRelation = function () {
  return _relation.apply(this, arguments);
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

////////////////////////////////////////////////////////////////////////////////
/// some more documentation
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_create_graph_example1
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_general_graph_create_graph_example2
////////////////////////////////////////////////////////////////////////////////


