/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;

const movieCollectionName = "MOVIE";

const cleanup = function() {
  db._drop(movieCollectionName);
};

const createData = function() {
  let c = db._createDocumentCollection(movieCollectionName);

  let data = [];
  for(let i = 1; i <= 321; i++) {
    data.push({ test_node: null });
  }
  for(let i = 1; i <= 1679; i++) {
    data.push({ test_node: i});
  }
  c.insert(data);
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

      var actual = db._query(query, bindVars).toArray();
      assertEqual(actual.length, 0, "Expecting query result to contain 0 documents");
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(indexSkipRegressionSuite);

return jsunity.done();
