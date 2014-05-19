/*jslint indent: 2, nomen: true, maxlen: 80, sloppy: true */
/*global require, assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the general-graph class
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
/// @author Florian Bartels, Michael Hackstein
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("org/arangodb");
var db = arangodb.db;

var console = require("console");
var graph = require("org/arangodb/general-graph");

var print = arangodb.print;

var _ = require("underscore");

// -----------------------------------------------------------------------------
// --SECTION--                                                      graph module
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: general-graph Creation and edge definition
////////////////////////////////////////////////////////////////////////////////

function GeneralGraphCreationSuite() {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Graph Creation
////////////////////////////////////////////////////////////////////////////////

    test_undirectedRelationDefinition : function () {
	    var r;

      try {
        r = graph._undirectedRelationDefinition("relationName", ["vertexC1", "vertexC2"]);
      }
      catch (err) {
      }

      assertEqual(r, {
	      collection: "relationName",
	      from: ["vertexC1", "vertexC2"],
	      to: ["vertexC1", "vertexC2"]
      });

    },

	  test_undirectedRelationDefinitionWithSingleCollection : function () {
		  var r;

		  try {
			  r = graph._undirectedRelationDefinition("relationName", "vertexC1");
		  }
		  catch (err) {
			}

		  assertEqual(r, {
			  collection: "relationName",
			  from: ["vertexC1"],
			  to: ["vertexC1"]
		  });

	  },

	  test_undirectedRelationDefinitionWithMissingName : function () {
		  var r, exception;
			try {
			  r = graph._undirectedRelationDefinition("", ["vertexC1", "vertexC2"]);
		  }
		  catch (err) {
			  exception = err;
		  }

		  assertEqual(exception, "<relationName> must be a not empty string");

	  },

	  test_undirectedRelationDefinitionWithTooFewArgs : function () {
		  var r, exception;
		  try {
			  r = graph._undirectedRelationDefinition(["vertexC1", "vertexC2"]);
		  }
		  catch (err) {
			  exception = err;
		  }

		  assertEqual(exception, "method _undirectedRelationDefinition expects 2 arguments");

	  },

	  test_undirectedRelationDefinitionWithInvalidSecondArg : function () {
		  var r, exception;
		  try {
			  r = graph._undirectedRelationDefinition("name", {"vertexC1" : "vertexC2"});
		  }
		  catch (err) {
			  exception = err;
		  }

		  assertEqual(exception, "<vertexCollections> must be a not empty string or array");

	  },

	  test_directedRelationDefinition : function () {
		  var r;

		  try {
			  r = graph._directedRelationDefinition("relationName",
				  ["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"]);
		  }
		  catch (err) {
		  }

		  assertEqual(r, {
			  collection: "relationName",
			  from: ["vertexC1", "vertexC2"],
			  to: ["vertexC3", "vertexC4"]
		  });

	  },

	  test_directedRelationDefinitionWithMissingName : function () {
		  var r, exception;
		  try {
			  r = graph._directedRelationDefinition("",
				  ["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"]);
		  }
		  catch (err) {
			  exception = err;
		  }

		  assertEqual(exception, "<relationName> must be a not empty string");

	  },

	  test_directedRelationDefinitionWithTooFewArgs : function () {
		  var r, exception;
		  try {
			  r = graph._directedRelationDefinition(["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"]);
		  }
		  catch (err) {
			  exception = err;
		  }

		  assertEqual(exception, "method _undirectedRelationDefinition expects 3 arguments");

	  },

	  test_directedRelationDefinitionWithInvalidSecondArg : function () {
		  var r, exception;
		  try {
			  r = graph._directedRelationDefinition("name", {"vertexC1" : "vertexC2"}, "");
		  }
		  catch (err) {
			  exception = err;
		  }

		  assertEqual(exception, "<fromVertexCollections> must be a not empty string or array");

	  },

	  test_directedRelationDefinitionWithInvalidThirdArg : function () {
		  var r, exception;
		  try {
			  r = graph._directedRelationDefinition("name", ["vertexC1", "vertexC2"], []);
		  }
		  catch (err) {
			  exception = err;
		  }

		  assertEqual(exception, "<toVertexCollections> must be a not empty string or array");

	  },

	  testEdgeDefinitions : function () {


		  //with empty args
		  assertEqual(graph.edgeDefinitions(), []);

		  //with args
		  assertEqual(graph.edgeDefinitions(
			  graph._undirectedRelationDefinition("relationName", "vertexC1"),
			  graph._directedRelationDefinition("relationName",
				  ["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"])
		  ), [
			  {
				  collection: "relationName",
				  from: ["vertexC1"],
				  to: ["vertexC1"]
			  },
			  {
				  collection: "relationName",
				  from: ["vertexC1", "vertexC2"],
				  to: ["vertexC3", "vertexC4"]
			  }
		  ]);

	  }

	  /*test_create : function () {


		  var a = graph._create(
			  "bla3",
			  graph.edgeDefinitions(
			    graph._undirectedRelationDefinition("relationName", "vertexC1"),
			    graph._directedRelationDefinition("relationName",
				  ["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"]
			    )
		    )
		  );
		  assertTrue(true);



	  }*/

  };

}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Simple Queries
// -----------------------------------------------------------------------------

function GeneralGraphAQLQueriesSuite() {

  var dropInclExcl = function() {
    var col = db._collection("_graphs");
    try {
      col.remove("graph");
    } catch (e) {
      return;
    }
    var colList = ["v1", "v2", "v3", "included", "excluded"];
    _.each(colList, function(c) {
      var colToClear = db._collection(c);
      if (col) {
        colToClear.truncate();
      }
    });
  };

  var createInclExcl = function() {
    dropInclExcl();
    var inc = graph._directedRelationDefinition("included", ["v1"], ["v1", "v2"]);
    var exc = graph._directedRelationDefinition("excluded", ["v1"], ["v3"]);
    var g = graph._create("graph", [inc, exc]);
    return g;
  };

  var findIdInResult = function(result, id) {
    return _.some(result, function(i) {
      return i._id === id; 
    });
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict construct on edges
////////////////////////////////////////////////////////////////////////////////

    test_edges: function() {
      var g = createInclExcl();
      var incEdge1 = g.included.save(
        "v1/1",
        "v2/1",
        {
          included: true
        }
      )._id;
      var incEdge2 = g.included.save(
        "v1/2",
        "v1/1",
        {
          included: true
        }
      )._id;
      var incEdge3 = g.excluded.save(
        "v1/1",
        "v3/1",
        {
          included: true
        }
      )._id;
      var query = g._edges("v1/1");
      assertEqual(query.printQuery(), "GRAPH_EDGES("
        + "@graphName,@startVertex_0,any)");
      var bindVars = query.bindVars;
      assertEqual(bindVars.graphName, "graph");
      assertEqual(bindVars.startVertex_0, "v1/1");
      /*
      var result = query.toArray();
      assertEqual(result.length, 3);
      assertTrue(findIdInResult(result, incEdge1));
      assertTrue(findIdInResult(result, incEdge2));
      assertFalse(findIdInResult(result, incEdge3));
      */
    },

    test_restrictOnEdges: function() {
      var g = createInclExcl();
      var incEdge1 = g.included.save(
        "v1/1",
        "v2/1",
        {
          included: true
        }
      )._id;
      var incEdge2 = g.included.save(
        "v1/2",
        "v1/1",
        {
          included: true
        }
      )._id;
      var excEdge = g.excluded.save(
        "v1/1",
        "v3/1",
        {
          included: false
        }
      )._id;
      var result = g._edges("v1/1").restrict("included");
      assertEqual(result.length, 2);
      assertTrue(findIdInResult(result, incEdge1));
      assertTrue(findIdInResult(result, incEdge2));
      assertFalse(findIdInResult(result, excEdge));
      dropInclExcl();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict construct on inEdges
////////////////////////////////////////////////////////////////////////////////

    test_restrictOnInEdges: function() {
      var g = createInclExcl();
      var excEdge1 = g.included.save(
        "v1/1",
        "v2/1",
        {
          included: false
        }
      )._id;
      var incEdge = g.included.save(
        "v1/2",
        "v1/1",
        {
          included: true
        }
      )._id;
      var excEdge2 = g.excluded.save(
        "v1/1",
        "v3/1",
        {
          included: false
        }
      )._id;
      var result = g._inEdges("v1/1").restrict("included");
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result, incEdge));
      assertFalse(findIdInResult(result, excEdge1));
      assertFalse(findIdInResult(result, excEdge2));
      dropInclExcl();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: restrict construct on outEdges
////////////////////////////////////////////////////////////////////////////////

    test_restrictOnOutEdges: function() {
      var g = createInclExcl();
      var incEdge = g.included.save(
        "v1/1",
        "v2/1",
        {
          included: true
        }
      )._id;
      var excEdge1 = g.included.save(
        "v1/2",
        "v1/1",
        {
          included: false
        }
      )._id;
      var excEdge2 = g.excluded.save(
        "v1/1",
        "v3/1",
        {
          included: false
        }
      )._id;
      var result = g._outEdges("v1/1").restrict("included");
      assertEqual(result.length, 1);
      assertTrue(findIdInResult(result, incEdge));
      assertFalse(findIdInResult(result, excEdge1));
      assertFalse(findIdInResult(result, excEdge2));
      dropInclExcl();
   },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: filter construct on Edges
////////////////////////////////////////////////////////////////////////////////
   
   test_filterOnEdges: function() {

   }

  };


}



// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(GeneralGraphCreationSuite);
jsunity.run(GeneralGraphAQLQueriesSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
