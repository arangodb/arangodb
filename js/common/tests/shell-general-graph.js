/*jslint indent: 2, nomen: true, maxlen: 80, sloppy: true */
/*global require, assertEqual, assertTrue */

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
/// @author Florian Bartels
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("org/arangodb");

var console = require("console");
var graph = require("org/arangodb/general-graph")

var print = arangodb.print;

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

	  },


	  test_create : function () {
			try {
		    arangodb.db._collection("_graphs").remove("_graphs/bla3")
			} catch (err) {

			}

		  var a = graph._create(
			  "bla3",
			  graph.edgeDefinitions(
			    graph._undirectedRelationDefinition("relationName", "vertexC1"),
			    graph._directedRelationDefinition("relationName",
				  ["vertexC1", "vertexC2"], ["vertexC3", "vertexC4"]
			    )
		    )
		  );
		  require("console").log(JSON.stringify(a));

		  assertEqual(a, "");



	  }


  }

}


// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(GeneralGraphCreationSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
