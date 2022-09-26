/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db,
  indexId;

// This example was produced by Jan Steeman to reproduce a
// crash in the TraversalExecutor code
const movieCollectionName = "MOVIE";

var cleanup = function() {
  db._drop(movieCollectionName);
};

var createData = function() {
  db._createDocumentCollection(movieCollectionName, {});

  var data = [];

  for(let i = 1; i <= 321; i++) {
    data.push({ test_node: null});
  }
  for(let i = 1; i <= 1679; i++) {
    data.push({ test_node: i});
  }
  db._collection(movieCollectionName).save(data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function indexSkipRegressionSuite() {
  return {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function() {
      cleanup();
      createData();
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
    testIndexSkipCrashes: function() {
      const query = `FOR doc IN @@value0
                         FILTER doc['test_node'] != @value1
			 SORT doc._key ASC
			 LIMIT 1680, 10
                         RETURN doc`;

      const bindVars = {
        "@value0": movieCollectionName,
        "value1": null
      };

      var actual = db._query(query, bindVars);
      var foo = actual.toArray();
      assertEqual(foo.length, 0, "Expecting query result to contain 0 documents");
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(indexSkipRegressionSuite);

return jsunity.done();
