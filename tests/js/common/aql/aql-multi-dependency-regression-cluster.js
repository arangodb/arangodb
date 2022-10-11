/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db,
  indexId;

// This example was produced by Jan Steeman to reproduce a
// crash in the TraversalExecutor code
const collectionName = "SubqueryChaosCollection2";

var cleanup = function() {
  db._drop(collectionName);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function multiDependencyRegressionSuite() {
  return {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function() {
      cleanup();
      let c = db._create(collectionName, { numberOfShards: 5 });
      
      const docs = [];
      for (let i = 1; i < 11; ++i) {
        docs.push({ value: i });
      }
      c.save(docs);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function() {
      cleanup();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief 
    ////////////////////////////////////////////////////////////////////////////////
    testMultiDependencyRegression: function() {
      const query = `
	  LET sq1 = (FOR fv2 IN 1..10
	    LET sq3 = (FOR fv4 IN ${collectionName}
	      UPDATE { _key: fv4._key } WITH {updated: true} IN  ${collectionName}
	      LIMIT 10,10
	      COLLECT WITH COUNT INTO counter
	      RETURN {counter})
      LIMIT 5,3
	    RETURN {fv2, sq3})
	  RETURN sq1`;

      var actual = db._query(query, {});
      assertEqual(actual.toArray(), [
        [
          { fv2: 6, sq3: [{ counter: 0 }] },
          { fv2: 7, sq3: [{ counter: 0 }] },
          { fv2: 8, sq3: [{ counter: 0 }] }
        ]
      ]);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(multiDependencyRegressionSuite);

return jsunity.done();
