/*jshint strict: false */
/*global require, exports*/

////////////////////////////////////////////////////////////////////////////////
/// @brief AQL Query generator for graph module
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein 
/// @author Copyright 2011-2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


(function() {
  "use strict";

// -----------------------------------------------------------------------------
// --SECTION--                             Fluent AQL Interface
// -----------------------------------------------------------------------------
  var arangodb = require("org/arangodb"),
    internal = require("internal"),
    db = internal.db,
    ArangoError = arangodb.ArangoError,
    errors = arangodb.errors,
    _ = require("underscore");

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
  /// @startDocuBlock JSF_general_graph_example_description
  ///
  /// For many of the following functions *examples* can be passed in as a parameter.
  /// *Examples* are used to filter the result set for objects that match the conditions.
  /// These *examples* can have the following values:
  ///
  /// * *null*, there is no matching executed all found results are valid.
  /// * A *string*, only results are returned, which *_id* equal the value of the string
  /// * An example *object*, defining a set of attributes.
  ///     Only results having these attributes are matched.
  /// * A *list* containing example *objects* and/or *strings*.
  ///     All results matching at least one of the elements in the list are returned.
  ///
  /// @endDocuBlock
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
      err.errorNum = errors.ERROR_BAD_PARAMETER.code;
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
    this.bindVars["options_" + this.stack.length] = this.options;
    var stmt = new AQLStatement(query, "edge");
    this.stack.push(stmt);
    this.lastVar = edgeName;
    this._path.push(edgeName);
    this._pathEdges.push(edgeName);
    return this;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_general_graph_fluent_aql_edges
  /// @brief Select all edges for the vertices selected before.
  ///
  /// `graph_query.edges(examples)`
  ///
  /// Creates an AQL statement to select all edges for each of the vertices selected
  /// in the step before.
  /// This will include *inbound* as well as *outbound* edges.
  /// The resulting set of edges can be filtered by defining one or more *examples*.
  ///
  /// The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
  /// parameter vertexExamplex, *m* the average amount of edges of a vertex and *x* the maximal depths.
  /// Hence the default call would have a complexity of **O(n\*m)**;
  ///
  /// @PARAMS
  ///
  /// @PARAM{examples, object, optional}
  /// See [Definition of examples](#definition_of_examples)
  ///
  /// @EXAMPLES
  ///
  /// To request unfiltered edges:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesUnfiltered}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  ///   query.edges().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered edges by a single example:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesFilteredSingle}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  ///   query.edges({type: "married"}).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered edges by multiple examples:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesFilteredMultiple}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  ///   query.edges([{type: "married"}, {type: "friend"}]).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
  ///
  ////////////////////////////////////////////////////////////////////////////////

  AQLGenerator.prototype.edges = function(example) {
    this._addToPrint("edges", example);
    return this._edges(example, {direction: "any"});
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_general_graph_fluent_aql_outEdges
  /// @brief Select all outbound edges for the vertices selected before.
  ///
  /// `graph_query.outEdges(examples)`
  ///
  /// Creates an AQL statement to select all *outbound* edges for each of the vertices selected
  /// in the step before.
  /// The resulting set of edges can be filtered by defining one or more *examples*.
  ///
  /// @PARAMS
  ///
  /// @PARAM{examples, object, optional}
  /// See [Definition of examples](#definition_of_examples)
  ///
  /// @EXAMPLES
  ///
  /// To request unfiltered outbound edges:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesUnfiltered}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  ///   query.outEdges().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered outbound edges by a single example:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesFilteredSingle}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  ///   query.outEdges({type: "married"}).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered outbound edges by multiple examples:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesFilteredMultiple}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  ///   query.outEdges([{type: "married"}, {type: "friend"}]).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
  ///
  ////////////////////////////////////////////////////////////////////////////////

  AQLGenerator.prototype.outEdges = function(example) {
    this._addToPrint("outEdges", example);
    return this._edges(example, {direction: "outbound"});
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_general_graph_fluent_aql_inEdges
  /// @brief Select all inbound edges for the vertices selected before.
  ///
  /// `graph_query.inEdges(examples)`
  ///
  ///
  /// Creates an AQL statement to select all *inbound* edges for each of the vertices selected
  /// in the step before.
  /// The resulting set of edges can be filtered by defining one or more *examples*.
  ///
  /// @PARAMS
  ///
  /// @PARAM{examples, object, optional}
  /// See [Definition of examples](#definition_of_examples)
  ///
  /// @EXAMPLES
  ///
  /// To request unfiltered inbound edges:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLInEdgesUnfiltered}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  ///   query.inEdges().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered inbound edges by a single example:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLInEdgesFilteredSingle}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  ///   query.inEdges({type: "married"}).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered inbound edges by multiple examples:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLInEdgesFilteredMultiple}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
  ///   query.inEdges([{type: "married"}, {type: "friend"}]).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
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
    this._pathVertices.push(vertexName);
    return this;

  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_general_graph_fluent_aql_vertices
  /// @brief Select all vertices connected to the edges selected before.
  ///
  /// `graph_query.vertices(examples)`
  ///
  /// Creates an AQL statement to select all vertices for each of the edges selected
  /// in the step before.
  /// This includes all vertices contained in *_from* as well as *_to* attribute of the edges.
  /// The resulting set of vertices can be filtered by defining one or more *examples*.
  ///
  /// @PARAMS
  ///
  /// @PARAM{examples, object, optional}
  /// See [Definition of examples](#definition_of_examples)
  ///
  /// @EXAMPLES
  ///
  /// To request unfiltered vertices:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesUnfiltered}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.vertices().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered vertices by a single example:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesFilteredSingle}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.vertices({name: "Alice"}).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered vertices by multiple examples:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesFilteredMultiple}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.vertices([{name: "Alice"}, {name: "Charly"}]).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
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
  /// @startDocuBlock JSF_general_graph_fluent_aql_fromVertices
  /// @brief Select all source vertices of the edges selected before.
  ///
  /// `graph_query.fromVertices(examples)`
  ///
  /// Creates an AQL statement to select the set of vertices where the edges selected
  /// in the step before start at.
  /// This includes all vertices contained in *_from* attribute of the edges.
  /// The resulting set of vertices can be filtered by defining one or more *examples*.
  ///
  /// @PARAMS
  ///
  /// @PARAM{examples, object, optional}
  /// See [Definition of examples](#definition_of_examples)
  ///
  /// @EXAMPLES
  ///
  /// To request unfiltered source vertices:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesUnfiltered}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.fromVertices().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered source vertices by a single example:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesFilteredSingle}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.fromVertices({name: "Alice"}).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered source vertices by multiple examples:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesFilteredMultiple}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.fromVertices([{name: "Alice"}, {name: "Charly"}]).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
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
  /// @startDocuBlock JSF_general_graph_fluent_aql_toVertices
  /// @brief Select all vertices targeted by the edges selected before.
  ///
  /// `graph_query.toVertices(examples)`
  ///
  /// Creates an AQL statement to select the set of vertices where the edges selected
  /// in the step before end in.
  /// This includes all vertices contained in *_to* attribute of the edges.
  /// The resulting set of vertices can be filtered by defining one or more *examples*.
  ///
  /// @PARAMS
  ///
  /// @PARAM{examples, object, optional}
  /// See [Definition of examples](#definition_of_examples)
  ///
  /// @EXAMPLES
  ///
  /// To request unfiltered target vertices:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToVerticesUnfiltered}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.toVertices().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered target vertices by a single example:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToVerticesFilteredSingle}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.toVertices({name: "Bob"}).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered target vertices by multiple examples:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToVerticesFilteredMultiple}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.toVertices([{name: "Bob"}, {name: "Diana"}]).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
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
  /// @startDocuBlock JSF_general_graph_fluent_aql_path
  /// @brief The result of the query is the path to all elements.
  ///
  /// `graph_query.path()`
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
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices({name: "Alice"});
  ///   query.outEdges().toVertices().path().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// When requesting neighbors the path to these neighbors is expanded:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLPathNeighbors}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices({name: "Alice"});
  ///   query.neighbors().path().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
  ///
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
  /// @startDocuBlock JSF_general_graph_fluent_aql_neighbors
  /// @brief Select all neighbors of the vertices selected in the step before.
  ///
  /// `graph_query.neighbors(examples, options)`
  ///
  /// Creates an AQL statement to select all neighbors for each of the vertices selected
  /// in the step before.
  /// The resulting set of vertices can be filtered by defining one or more *examples*.
  ///
  /// @PARAMS
  ///
  /// @PARAM{examples, object, optional}
  /// See [Definition of examples](#definition_of_examples)
  ///
  /// @PARAM{options, object, optional}
  ///   An object defining further options. Can have the following values:
  ///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
  ///   * *edgeExamples*: Filter the edges to be followed, see [Definition of examples](#definition_of_examples)
  ///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
  ///       considered to be on the path.
  ///   * *vertexCollectionRestriction* : One or a list of vertex-collection names that should be
  ///       considered on the intermediate vertex steps.
  ///   * *minDepth*: Defines the minimal number of intermediate steps to neighbors (default is 1).
  ///   * *maxDepth*: Defines the maximal number of intermediate steps to neighbors (default is 1).
  ///
  /// @EXAMPLES
  ///
  /// To request unfiltered neighbors:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsUnfiltered}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices({name: "Alice"});
  ///   query.neighbors().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered neighbors by a single example:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsFilteredSingle}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices({name: "Alice"});
  ///   query.neighbors({name: "Bob"}).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// To request filtered neighbors by multiple examples:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsFilteredMultiple}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.vertices([{name: "Bob"}, {name: "Charly"}]).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
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
    opts.neighborExamples = ex;
    this.bindVars["options_" + this.stack.length] = opts;
    var stmt = new AQLStatement(query, "neighbor");
    this.stack.push(stmt);
    this.lastVar = resultName + ".vertex";
    this._path.push(resultName + ".path");
    this._pathVertices.push("SLICE(" + resultName + ".path.vertices, 1)");
    this._pathEdges.push(resultName + ".path.edges");
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
  /// @startDocuBlock JSF_general_graph_fluent_aql_restrict
  /// @brief Restricts the last statement in the chain to return
  /// only elements of a specified set of collections
  ///
  /// `graph_query.restrict(restrictions)`
  ///
  /// By default all collections in the graph are searched for matching elements
  /// whenever vertices and edges are requested.
  /// Using *restrict* after such a statement allows to restrict the search
  /// to a specific set of collections within the graph.
  /// Restriction is only applied to this one part of the query.
  /// It does not effect earlier or later statements.
  ///
  /// @PARAMS
  ///
  /// @PARAM{restrictions, array, optional}
  /// Define either one or a list of collections in the graph.
  /// Only elements from these collections are taken into account for the result.
  ///
  /// @EXAMPLES
  ///
  /// Request all directly connected vertices unrestricted:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnrestricted}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices({name: "Alice"});
  ///   query.edges().vertices().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// Apply a restriction to the directly connected vertices:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLRestricted}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices({name: "Alice"});
  ///   query.edges().vertices().restrict("female").toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// Restriction of a query is only valid for collections known to the graph:
  //
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLRestrictedUnknown}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices({name: "Alice"});
  ///   query.edges().vertices().restrict(["female", "male", "products"]).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
  ///
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
  /// @startDocuBlock JSF_general_graph_fluent_aql_filter
  /// @brief Filter the result of the query
  ///
  /// `graph_query.filter(examples)`
  ///
  /// This can be used to further specfiy the expected result of the query.
  /// The result set is reduced to the set of elements that matches the given *examples*.
  ///
  /// @PARAMS
  ///
  /// @PARAM{examples, object, optional}
  /// See [Definition of examples](#definition_of_examples)
  ///
  /// @EXAMPLES
  ///
  /// Request vertices unfiltered:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnfilteredVertices}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.toVertices().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// Request vertices filtered:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFilteredVertices}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.toVertices().filter({name: "Alice"}).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// Request edges unfiltered:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnfilteredEdges}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.toVertices().outEdges().toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// Request edges filtered:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFilteredEdges}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._edges({type: "married"});
  ///   query.toVertices().outEdges().filter({type: "married"}).toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
  ///
  ////////////////////////////////////////////////////////////////////////////////

  AQLGenerator.prototype.filter = function(example) {
    this._addToPrint("filter", example);
    this._clearCursor();
    var ex = [];
    if (Object.prototype.toString.call(example) !== "[object Array]") {
      if (Object.prototype.toString.call(example) !== "[object Object]") {
        var err = new ArangoError();
        err.errorNum = errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT.code;
        err.errorMessage = errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT.message;
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
  /// @startDocuBlock JSF_general_graph_fluent_aql_toArray
  /// @brief Returns an array containing the complete result.
  ///
  /// `graph_query.toArray()`
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
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices();
  ///   query.toArray();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
  ///
  ////////////////////////////////////////////////////////////////////////////////

  AQLGenerator.prototype.toArray = function() {
    this._createCursor();

    return this.cursor.toArray();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_general_graph_fluent_aql_count
  /// @brief Returns the number of returned elements if the query is executed.
  ///
  /// `graph_query.count()`
  ///
  /// This function determines the amount of elements to be expected within the result of the query.
  /// It can be used at the beginning of execution of the query
  /// before using *next()* or in between *next()* calls.
  /// The query object maintains a cursor of the query for you.
  /// *count()* does not change the cursor position.
  ///
  /// @EXAMPLES
  ///
  /// To count the number of matched elements:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLCount}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices();
  ///   query.count();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
  //
  ////////////////////////////////////////////////////////////////////////////////

  AQLGenerator.prototype.count = function() {
    this._createCursor();
    return this.cursor.count();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_general_graph_fluent_aql_hasNext
  /// @brief Checks if the query has further results.
  ///
  /// `graph_query.hasNext()`
  ///
  /// The generated statement maintains a cursor for you.
  /// If this cursor is already present *hasNext()* will
  /// use this cursors position to determine if there are
  /// further results available.
  /// If the query has not yet been executed *hasNext()*
  /// will execute it and create the cursor for you.
  ///
  /// @EXAMPLES
  ///
  /// Start query execution with hasNext:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLHasNext}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices();
  ///   query.hasNext();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// Iterate over the result as long as it has more elements:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLHasNextIteration}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices();
  /// | while (query.hasNext()) {
  /// |   var entry = query.next();
  /// |   // Do something with the entry
  ///   }
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
  ///
  ////////////////////////////////////////////////////////////////////////////////

  AQLGenerator.prototype.hasNext = function() {
    this._createCursor();
    return this.cursor.hasNext();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_general_graph_fluent_aql_next
  /// @brief Request the next element in the result.
  ///
  /// `graph_query.next()`
  ///
  /// The generated statement maintains a cursor for you.
  /// If this cursor is already present *next()* will
  /// use this cursors position to deliver the next result.
  /// Also the cursor position will be moved by one.
  /// If the query has not yet been executed *next()*
  /// will execute it and create the cursor for you.
  /// It will throw an error of your query has no further results.
  ///
  /// @EXAMPLES
  ///
  /// Request some elements with next:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNext}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices();
  ///   query.next();
  ///   query.next();
  ///   query.next();
  ///   query.next();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  ///
  /// The cursor is recreated if the query is changed:
  ///
  /// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNextRecreate}
  ///   var examples = require("org/arangodb/graph-examples/example-graph.js");
  ///   var graph = examples.loadGraph("social");
  ///   var query = graph._vertices();
  ///   query.next();
  ///   query.edges();
  ///   query.next();
  /// @END_EXAMPLE_ARANGOSH_OUTPUT
  /// @endDocuBlock
  ////////////////////////////////////////////////////////////////////////////////

  AQLGenerator.prototype.next = function() {
    this._createCursor();
    return this.cursor.next();
  };

  exports.AQLGenerator = AQLGenerator;
  exports.transformExample = transformExample;
  exports.stringToArray = stringToArray;
}());
