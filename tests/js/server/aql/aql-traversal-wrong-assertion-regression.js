/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
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
/// @author Markus Pfeiffer
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db,
  indexId;

// This example was produced by Jan Steeman to reproduce a
// crash in the TraversalExecutor code
const collectionName = "RegressionVerts";
const edgeName = "RegressionEdges";

var cleanup = function() {
  db._drop();
  db._drop(collectionName);
  db._drop(edgeName);
};

var createGraph = function() {
  db._createDocumentCollection(collectionName);
  db._createEdgeCollection(edgeName);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function traversalPruneAssertionRegressionSuite() {
  return {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function() {
      cleanup();
      createGraph();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function() {
      cleanup();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test object access for path object
    ////////////////////////////////////////////////////////////////////////////////
    testTraversalNonAssertion: function() {
      const query = `WITH @@V, @@E
                       FOR v IN ANY @start @@E
		         PRUNE v
			 RETURN true`;

      const bindVars = {
        "@V": collectionName,
        "@E": edgeName,
        "start": collectionName + "/S"
      };

      var actual = db._query(query, bindVars);
      assertEqual(actual.toArray(), []);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(traversalPruneAssertionRegressionSuite);
return jsunity.done();
