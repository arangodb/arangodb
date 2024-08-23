/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Markus Pfeiffer
// //////////////////////////////////////////////////////////////////////////////

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
