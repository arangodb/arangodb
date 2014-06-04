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
/// @author Florian Bartels, Michael Hackstein, Guido Schwab
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


var arangodb = require("org/arangodb"),
  internal = require("internal"),
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
    throw "<" + name + "> must be an ArangoCollection ";
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

var getGraphCollection = function() {
  var gCol = db._graphs;
  if (gCol === null || gCol === undefined) {
    throw "_graphs collection does not exist.";
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
  throw "Invalid example type. Has to be String, Array or Object";
};

var checkAllowsRestriction = function(colList, rest, msg) {
  var unknown = [];
  _.each(rest, function(r) {
    if (!colList[r]) {
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


// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/general-graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                             Fluent AQL Interface
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                AQL Generator for fluent interface
// -----------------------------------------------------------------------------

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
  this.bindVars["options_" + this.stack.length] = this.options;
  var stmt = new AQLStatement(query, "edge");
  this.stack.push(stmt);
  this.lastVar = edgeName;
  this._path.push(edgeName);
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_edges
/// @brief select all edges for the vertices selected before
/// 
/// @FUN{graph-query.edges(@FA{examples})}
///
/// Creates an AQL statement to select all edges for each of the vertices selected
/// in the step before.
/// This will include `inbound` as well as `outbound` edges.
/// The resulting set of edges can be filtered by defining one or more @FA{examples}.
///
/// @FA{examples} can have the following values:
/// 
///   * Empty, there is no matching executed all edges are valid.
///   * A String, only the edge having this value as it's id is returned.
///   * An example object, defining a set of attributes.
///       Only edges having these attributes are matched.
///   * A list containing example objects and/or strings.
///       All edges matching at least one of the elements in the list are returned.
///
/// @EXAMPLES
///
/// To request unfiltered edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.edges().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered edges by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.edges({type: "married"}).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered edges by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.edges([{type: "married"}, {type: "friend"}]).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.edges = function(example) {
  this._addToPrint("edges", example);
  return this._edges(example, {direction: "any"});
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_outEdges
/// @brief select all outbound edges for the vertices selected before
/// 
/// @FUN{graph-query.outEdges(@FA{examples})}
///
/// Creates an AQL statement to select all `outbound` edges for each of the vertices selected
/// in the step before.
/// The resulting set of edges can be filtered by defining one or more @FA{examples}.
///
/// @FA{examples} can have the following values:
/// 
///   * Empty, there is no matching executed all edges are valid.
///   * A String, only the edge having this value as it's id is returned.
///   * An example object, defining a set of attributes.
///       Only edges having these attributes are matched.
///   * A list containing example objects and/or strings.
///       All edges matching at least one of the elements in the list are returned.
///
/// @EXAMPLES
///
/// To request unfiltered outbound edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.outEdges().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered outbound edges by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.outEdges({type: "married"}).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered outbound edges by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.outEdges([{type: "married"}, {type: "friend"}]).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.outEdges = function(example) {
  this._addToPrint("outEdges", example);
  return this._edges(example, {direction: "outbound"});
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_inEdges
/// @brief select all inbound edges for the vertices selected before
/// 
/// @FUN{graph-query.inEdges(@FA{examples})}
///
/// Creates an AQL statement to select all `inbound` edges for each of the vertices selected
/// in the step before.
/// The resulting set of edges can be filtered by defining one or more @FA{examples}.
///
/// @FA{examples} can have the following values:
/// 
///   * Empty, there is no matching executed all edges are valid.
///   * A String, only the edge having this value as it's id is returned.
///   * An example object, defining a set of attributes.
///       Only edges having these attributes are matched.
///   * A list containing example objects and/or strings.
///       All edges matching at least one of the elements in the list are returned.
///
/// @EXAMPLES
///
/// To request unfiltered inbound edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLInEdgesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.inEdges().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered inbound edges by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLInEdgesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.inEdges({type: "married"}).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered inbound edges by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLInEdgesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.inEdges([{type: "married"}, {type: "friend"}]).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
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

AQLGenerator.prototype._vertices = function(example, options) {
  this._clearCursor();
  this.options = options || {};
  var ex = transformExample(example);
  var vertexName = "vertices_" + this.stack.length;
  var query = "FOR " + vertexName
    + " IN GRAPH_VERTICES(@graphName,@vertexExample_"
    + this.stack.length + ',@options_'
    + this.stack.length + ')';
  this.bindVars["vertexExample_" + this.stack.length] = ex;
  this.bindVars["options_" + this.stack.length] = this.options;
  var stmt = new AQLStatement(query, "vertex");
  this.stack.push(stmt);
  this.lastVar = vertexName;
  this._path.push(vertexName);
  return this;

};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_vertices
/// @brief select all vertices connected to the edges selected before
/// 
/// @FUN{graph-query.vertices(@FA{examples})}
///
/// Creates an AQL statement to select all vertices for each of the edges selected
/// in the step before.
/// This includes all vertices contained in `_from` as well as `_to` attribute of the edges.
/// The resulting set of vertices can be filtered by defining one or more @FA{examples}.
///
/// @FA{examples} can have the following values:
/// 
///   * Empty, there is no matching executed all vertices are valid.
///   * A String, only the vertex having this value as it's id is returned.
///   * An example object, defining a set of attributes.
///       Only vertices having these attributes are matched.
///   * A list containing example objects and/or strings.
///       All vertices matching at least one of the elements in the list are returned.
///
/// @EXAMPLES
///
/// To request unfiltered vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.vertices().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered vertices by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.vertices({name: "Alice"}).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered vertices by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.vertices([{name: "Alice"}, {name: "Charly"}]).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.vertices = function(example) {
  this._addToPrint("vertices", example);
  if (!this.getLastVar()) {
    return this._vertices(example);
  }
  var edgeVar = this.getLastVar();
  this._vertices(example);
  var vertexVar = this.getLastVar();
  var query = "FILTER " + edgeVar
    + "._from == " + vertexVar
    + "._id || " + edgeVar
    + "._to == " + vertexVar
    + "._id";
  var stmt = new AQLStatement(query);
  this.stack.push(stmt);
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_fromVertices
/// @brief select all vertices where the edges selected before start
/// 
/// @FUN{graph-query.vertices(@FA{examples})}
///
/// Creates an AQL statement to select the set of vertices where the edges selected
/// in the step before start at.
/// This includes all vertices contained in `_from` attribute of the edges.
/// The resulting set of vertices can be filtered by defining one or more @FA{examples}.
///
/// @FA{examples} can have the following values:
/// 
///   * Empty, there is no matching executed all vertices are valid.
///   * A String, only the vertex having this value as it's id is returned.
///   * An example object, defining a set of attributes.
///       Only vertices having these attributes are matched.
///   * A list containing example objects and/or strings.
///       All vertices matching at least one of the elements in the list are returned.
///
/// @EXAMPLES
///
/// To request unfiltered starting vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.fromVertices().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered starting vertices by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.fromVertices({name: "Alice"}).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered starting vertices by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.fromVertices([{name: "Alice"}, {name: "Charly"}]).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.fromVertices = function(example) {
  this._addToPrint("fromVertices", example);
  if (!this.getLastVar()) {
    return this._vertices(example);
  }
  var edgeVar = this.getLastVar();
  this._vertices(example);
  var vertexVar = this.getLastVar();
  var query = "FILTER " + edgeVar
    + "._from == " + vertexVar
    + "._id";
  var stmt = new AQLStatement(query);
  this.stack.push(stmt);
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_toVertices
/// @brief select all vertices targeted by the edges selected before
/// 
/// @FUN{graph-query.vertices(@FA{examples})}
///
/// Creates an AQL statement to select the set of vertices where the edges selected
/// in the step before end in.
/// This includes all vertices contained in `_to` attribute of the edges.
/// The resulting set of vertices can be filtered by defining one or more @FA{examples}.
///
/// @FA{examples} can have the following values:
/// 
///   * Empty, there is no matching executed all vertices are valid.
///   * A String, only the vertex having this value as it's id is returned.
///   * An example object, defining a set of attributes.
///       Only vertices having these attributes are matched.
///   * A list containing example objects and/or strings.
///       All vertices matching at least one of the elements in the list are returned.
///
/// @EXAMPLES
///
/// To request unfiltered starting vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToVerticesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.toVertices().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered starting vertices by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToVerticesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.toVertices({name: "Alice"}).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered starting vertices by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToVerticesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.toVertices([{name: "Alice"}, {name: "Charly"}]).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.toVertices = function(example) {
  this._addToPrint("toVertices", example);
  if (!this.getLastVar()) {
    return this._vertices(example);
  }
  var edgeVar = this.getLastVar();
  this._vertices(example);
  var vertexVar = this.getLastVar();
  var query = "FILTER " + edgeVar
    + "._to == " + vertexVar
    + "._id";
  var stmt = new AQLStatement(query);
  this.stack.push(stmt);
  return this;
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
/// @fn JSF_general_graph_fluent_aql_path
/// @brief The result of the query is the path to all elements.
///
/// By defaut the result of the generated AQL query is the set of elements passing the last matches.
/// So having a `vertices()` query as the last step the result will be set of vertices.
/// Using `path()` as the last action before requesting the result
/// will modify the result such that the path required to find the set vertices is returned.
///
/// @EXAMPLES
///
/// Request the iteratively explored path using vertices and edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLPathSimple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices({name: "Alice"});
///   query.outEdges().toVertices().path().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// When requesting neighbors the path to these neighbors is expanded:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLPathNeighbors}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices({name: "Alice"});
///   query.neighbors().path().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.path = function() {
  this._clearCursor();
  var statement = new AQLStatement("", "path");
  this.stack.push(statement);
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_neighbors
/// @brief select all neighbors of the vertices selected in the step before.
/// 
/// @FUN{graph-query.neighbors(@FA{examples})}
///
/// Creates an AQL statement to select all neighbors for each of the vertices selected
/// in the step before.
/// The resulting set of vertices can be filtered by defining one or more @FA{examples}.
///
/// @FA{examples} can have the following values:
/// 
///   * Empty, there is no matching executed all vertices are valid.
///   * A String, only the vertex having this value as it's id is returned.
///   * An example object, defining a set of attributes.
///       Only vertices having these attributes are matched.
///   * A list containing example objects and/or strings.
///       All vertices matching at least one of the elements in the list are returned.
///
/// @EXAMPLES
///
/// To request unfiltered neighbors:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices({name: "Alice"});
///   query.neighbors().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered neighbors by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices({name: "Alice"});
///   query.neighbors({name: "Bob"}).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered neighbors by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._edges({type: "married"});
///   query.vertices([{name: "Bob"}, {name: "Charly"}]).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
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
    opts = _.clone(options);
  } else {
    opts = {};
  }
  opts.vertexExamples = ex;
  this.bindVars["options_" + this.stack.length] = opts;
  var stmt = new AQLStatement(query, "neighbor");
  this.stack.push(stmt);
  this.lastVar = resultName + ".vertex";
  this._path.push(resultName + ".path");
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
/// @fn JSF_general_graph_fluent_aql_restrict
/// @brief Restricts the last statement in the chain to return
///   only elements of a specified set of collections
///
/// By default all collections in the graph are searched for matching elements
/// whenever vertices and edges are requested.
/// Using `restrict` after such a statement allows to restrict the search
/// to a specific set of collections within the graph.
/// Restriction is only applied to this one part of the query.
/// It does not effect earlier or later statements.
/// 
/// @FA{restrictions} can have the following values:
///
/// * A string defining the name of one specific collection in the graph.
///   Only elements from this collection are used for matching
/// * A list of strings defining a set of collection names.
///   Elements from all collections in this set are used for matching
///
/// @EXAMPLES
///
/// Request all directly connected vertices unrestricted:
/// 
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnrestricted}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices({name: "Alice"});
///   query.edges().vertices().toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Apply a restriction to the directly connected vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLRestricted}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices({name: "Alice"});
///   query.edges().vertices().restrict("female").toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Restriction of a query is only valid for collections known to the graph:
//
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLRestricted}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices({name: "Alice"});
///   query.edges().vertices().restrict(["female", "male", "products"]).toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.restrict = function(restrictions) {
  this._addToPrint("restrict", restrictions);
  this._clearCursor();
  var rest = stringToArray(restrictions);
  var lastQueryInfo = this._getLastRestrictableStatementInfo();
  var lastQuery = lastQueryInfo.statement;
  var opts = lastQueryInfo.options;
  var restricts;
  if (lastQuery.isEdgeQuery()) {
    checkAllowsRestriction(
      this.graph.__edgeCollections,
      rest,
      "edge collections"
    );
    restricts = opts.edgeCollectionRestriction || [];
    opts.edgeCollectionRestriction = restricts.concat(restrictions);
  } else if (lastQuery.isVertexQuery() || lastQuery.isNeighborQuery()) {
    checkAllowsRestriction(
      this.graph.__vertexCollections,
      rest,
      "vertex collections"
    );
    restricts = opts.vertexCollectionRestriction || [];
    opts.vertexCollectionRestriction = restricts.concat(restrictions);
  }
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_filter
/// @brief TODO write
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.filter = function(example) {
  this._addToPrint("filter", example);
  this._clearCursor();
  var ex = [];
  if (Object.prototype.toString.call(example) !== "[object Array]") {
    if (Object.prototype.toString.call(example) !== "[object Object]") {
      throw "The example has to be an Object, or an Array";
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
  } else {
    query += " RETURN " + this.getLastVar();
  }
  return db._query(query, bindVars, {count: true});
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_toArray
/// @brief Returns an array containing the complete result.
///
/// This function executes the generated query and returns the
/// entire result as one array.
/// ToArray does not return the generated query anymore and
/// hence can only be the endpoint of a query.
/// However keeping a reference to the query before
/// executing allows to chain further statements to it.
///
/// @EXAMPLES
///
/// To collect the entire result of a query toArray can be used:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToArray}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices();
///   query.toArray();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.toArray = function() {
  this._createCursor();
  return this.cursor.toArray();
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_count
/// @brief Returns the number of returned elements if the query is executed.
///
/// This function determines the amount of elements to be expected within the result of the query.
/// It can be used at the beginning of execution of the query
/// before using `next()` or in between `next()` calls.
/// The query object maintains a cursor of the query for you.
/// `count()` does not change the cursor position.
///
/// @EXAMPLES
///
/// To count the number of matched elements: 
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToArray}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices();
///   query.count();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
//
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.count = function() {
  this._createCursor();
  return this.cursor.count();
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_hasNext
/// @brief Checks if the query has further results.
///
/// The generated statement maintains a cursor for you.
/// If this cursor is already present `hasNext()` will
/// use this cursors position to determine if there are
/// further results available.
/// If the query has not yet been executed `hasNext()`
/// will execute it and create the cursor for you.
///
/// @EXAMPLES
///
/// Start query execution with hasNext: 
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToArray}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices();
///   query.hasNext();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Iterate over the result as long as it has more elements:
/// 
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToArray}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices();
///   while(query.hasNext()) {
///     query.next();
///   }
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// 
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.hasNext = function() {
  this._createCursor();
  return this.cursor.hasNext();
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_fluent_aql_next
/// @brief Request the next element in the result
///
/// The generated statement maintains a cursor for you.
/// If this cursor is already present `next()` will
/// use this cursors position to deliver the next result.
/// Also the cursor position will be moved by one.
/// If the query has not yet been executed `next()`
/// will execute it and create the cursor for you.
/// It will throw an error of your query has no further results.
///
/// @EXAMPLES
///
/// Request some elements with next: 
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToArray}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices();
///   query.next();
///   query.next();
///   query.next();
///   query.next();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// The cursor is recreated if the query is changed.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToArray}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   var query = g._vertices();
///   query.next();
///   query.edges();
///   query.next();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.next = function() {
  this._createCursor();
  return this.cursor.next();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_general_graph_undirectedRelationDefinition
/// @brief define an undirected relation.
/// 
/// @FUN{general-graph._undirectedRelationDefinition(@FA{relationName}, @FA{vertexCollections})}
///
/// Defines an undirected relation with the name @FA{relationName} using the
/// list of @FA{vertexCollections}. This relation allows the user to store
/// edges in any direction between any pair of vertices within the
/// @FA{vertexCollections}.
///
/// @EXAMPLES
///
/// To define simple relation with only one vertex collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphUndirectedRelationDefinition1}
///   var graph = require("org/arangodb/general-graph");
///   graph._undirectedRelationDefinition("friend", "user");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To define a relation between several vertex collections:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphUndirectedRelationDefinition2}
///   var graph = require("org/arangodb/general-graph");
///   graph._undirectedRelationDefinition("marriage", ["female", "male"]);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
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
    if (i === 1) {return;}
    edgeDefinition.push(args[x]);
  });
};
////////////////////////////////////////////////////////////////////////////////
/// @brief create a new graph
////////////////////////////////////////////////////////////////////////////////


var _create = function (graphName, edgeDefinitions) {

  var gdb = getGraphCollection(),
    g,
    graphAlreadyExists = true,
    collections,
    err;
  if (!graphName) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_NAME.message;
    throw err;
  }
  if (!Array.isArray(edgeDefinitions) || edgeDefinitions.length === 0) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_EDGE_DEFINITION.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_EDGE_DEFINITION.message;
    throw err;
  }
  //check, if a collection is already used in a different edgeDefinition
  var tmpCollections = [];
  var tmpEdgeDefinitions = {};
  edgeDefinitions.forEach(
    function(edgeDefinition) {
      var col = edgeDefinition.collection;
      if (tmpCollections.indexOf(col) !== -1) {
        var err = new ArangoError();
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
              var err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code;
              err.errorMessage = col
                + arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message;
              throw err;
            }
          }
        }
      );
    }
  );

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
  this.__idsToRemove = [];
  this.__collectionsToLock = [];

  // fills this.__idsToRemove and this.__collectionsToLock
  var removeEdge = function (edgeId, options) {
    options = options || {};
    var edgeCollection = edgeId.split("/")[0];
    var graphs = getGraphCollection().toArray();
//    var result = db._remove(edgeId, options);
    self.__idsToRemove.push(edgeId);
    self.__collectionsToLock.push(edgeCollection);
//    var result = old_remove(edgeId, options);
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
                var edges = db[collection].toArray();
                edges.forEach(
                  function (edge) {
                    // if from is
                    if(self.__idsToRemove.indexOf(edge._id) === -1) {
                      if (edge._from === edgeId || edge._to === edgeId) {
                        removeEdge(edge._id, options);
                      }
/*
                      var newGraph = exports._graph(graph._key);
                      newGraph[collection].remove(edge._id, options);
*/
                    }
                  }
                );
              }
            }
          );
        }
      }
    );
    return;
  };


  _.each(vertexCollections, function(obj, key) {
    var result;
    var wrap = wrapCollection(obj);
    var old_remove = wrap.remove;
    wrap.remove = function(vertexId, options) {
      //delete all edges using the vertex in all graphs
      var graphs = getGraphCollection().toArray();
      var vertexCollectionName = vertexId.split("/")[0];
      self.__collectionsToLock.push(vertexCollectionName);
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
                  var edges = db._collection(collection).toArray();
                  edges.forEach(
                    function(edge) {
                      if (edge._from === vertexId || edge._to === vertexId) {
                        removeEdge(edge._id, options);
                      }
                    }
                  );
                }
              }
            );
          }
        }
      );

      try {
        db._executeTransaction({
          collections: {
            write: self.__collectionsToLock
          },
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
            ids: self.__idsToRemove,
            options: options,
            vertexId: vertexId
          }
        });
        result = true;
      } catch (e) {
        result = false;
      }
      self.__idsToRemove = [];
      self.__collectionsToLock = [];

      return result;
    };

    self[key] = wrap;
  });

  _.each(edgeCollections, function(obj, key) {
    var wrap = wrapCollection(obj);
    // save
    var old_save = wrap.save;
    wrap.save = function(from, to, data) {
      //check, if edge is allowed
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

    // remove
    wrap.remove = function(edgeId, options) {
      var result;
      //if _key make _id (only on 1st call)
      if (edgeId.indexOf("/") === -1) {
        edgeId = key + "/" + edgeId;
      }
//      return (removeEdge(edgeId, options));
      removeEdge(edgeId, options);

/*
      var collectionsToLock = [];
      self.__idsToRemove.forEach(
        function(idToRemove) {
          var collectionOfIdToRemove = idToRemove.split("/")[0];
          if(collectionsToLock.indexOf(collectionOfIdToRemove) === -1) {
            collectionsToLock.push(collectionOfIdToRemove);
          }
        }
      );
*/

      try {
        db._executeTransaction({
          collections: {
            write: self.__collectionsToLock
          },
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
            ids: self.__idsToRemove,
            options: options
          }
        });
        result = true;
      } catch (e) {
        result = false;
      }
      self.__idsToRemove = [];
      self.__collectionsToLock = [];
      return result;
    };

    self[key] = wrap;
  });
};



////////////////////////////////////////////////////////////////////////////////
/// @brief load a graph.
////////////////////////////////////////////////////////////////////////////////

var _graph = function(graphName) {

  var gdb = getGraphCollection(),
    g, collections;

  try {
    g = gdb.document(graphName);
  } 
  catch (e) {
    if (e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw e;
    }
    throw "graph " + graphName + " does not exist.";
  }

  collections = findOrCreateCollectionsByEdgeDefinitions(g.edgeDefinitions, true);

  return new Graph(graphName, g.edgeDefinitions, collections[0], collections[1]);
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
      if (graph.edgeDefinitions) {
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
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drop a graph.
////////////////////////////////////////////////////////////////////////////////

var _drop = function(graphId, dropCollections) {

  var gdb = getGraphCollection();

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
        var collection = edgeDefinition.collection;
        var graphs = getGraphCollection().toArray();
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
  if (vertexId.indexOf("/") === -1) {
    throw vertexId + " is not a valid id";
  }
  var collection = vertexId.split("/")[0];
  if (!db._collection(collection)) {
    throw collection + " does not exist.";
  }

  var edgeCollections = this._edgeCollections();
  var result = [];

  edgeCollections.forEach(
    function(edgeCollection) {
      result = result.concat(edgeCollection.edges(vertexId));
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief INEDGES(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._INEDGES = function(vertexId) {
  if (vertexId.indexOf("/") === -1) {
    throw vertexId + " is not a valid id";
  }
  var collection = vertexId.split("/")[0];
  if (!db._collection(collection)) {
    throw collection + " does not exist.";
  }

  var edgeCollections = this._edgeCollections();
  var result = [];


  edgeCollections.forEach(
    function(edgeCollection) {
      result = result.concat(edgeCollection.inEdges(vertexId));
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._OUTEDGES = function(vertexId) {
  if (vertexId.indexOf("/") === -1) {
    throw vertexId + " is not a valid id";
  }
  var collection = vertexId.split("/")[0];
  if (!db._collection(collection)) {
    throw collection + " does not exist.";
  }

  var edgeCollections = this._edgeCollections();
  var result = [];


  edgeCollections.forEach(
    function(edgeCollection) {
      result = result.concat(edgeCollection.outEdges(vertexId));
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _edges(edgeExample).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._edges = function(edgeExample) {
  var AQLStmt = new AQLGenerator(this);
  // If no direction is specified all edges are duplicated.
  // => For initial requests a direction has to be set
  return AQLStmt.outEdges(edgeExample);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _inEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._inEdges = function(vertexExample) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.edges(vertexExample, {direction : "inbound"});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _outEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._outEdges = function(vertexExample) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.edges(vertexExample, {direction : "outbound"});
};

/*
////////////////////////////////////////////////////////////////////////////////
/// @brief _vertices(edgeExample).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._vertices = function(vertexExamples) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.vertices(vertexExamples, {direction : "any"});
};

*/

////////////////////////////////////////////////////////////////////////////////
/// @brief _inEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._inVertices = function(vertexExamples) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.vertices(vertexExamples, {direction : "inbound"});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _outEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._outVertices = function(vertexExamples) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.vertices(vertexExamples, {direction : "outbound"});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _vertices(edgeExample||edgeId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._vertices = function(example) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.vertices(example);
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

Graph.prototype._neighbors = function(vertexExample, options) {
  var AQLStmt;

  if (! options) {
    options = { };
  }

  AQLStmt = new AQLGenerator(this);

  return AQLStmt.neighbors(vertexExample, options);
};



////////////////////////////////////////////////////////////////////////////////
/// @brief get common neighbors of two vertices in the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._listCommonNeighbors = function(vertex1Example, vertex2Example, options) {

  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_NEIGHBORS(@graphName"
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
/// @brief get amount of common neighbors of two vertices in the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._amountCommonNeighbors = function(vertex1Example, vertex2Example, options) {
  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_NEIGHBORS(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options'
    + ') FOR a in ATTRIBUTES(e) FOR b in ATTRIBUTES(e[a])  '
    + 'SORT  ATTRIBUTES(e)[0] RETURN [a, b, LENGTH(e[a][b]) ]';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
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
/// @brief get common properties of two vertices in the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._listCommonProperties = function(vertex1Example, vertex2Example, options) {

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
/// @brief get amount of common properties of two vertices in the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._amountCommonProperties = function(vertex1Example, vertex2Example, options) {
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
/// @brief print basic information for the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._PRINT = function(context) {
  var name = this.__name;
  var edgeDefs = printEdgeDefinitions(this.__edgeDefinitions);
  context.output += "[ Graph ";
  context.output += name;
  context.output += " EdgeDefinitions: ";
  internal.printRecursive(edgeDefs, context);
  context.output += " ]";
};

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
