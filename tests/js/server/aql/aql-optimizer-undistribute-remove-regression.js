/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangDB GmbH, Cologne, Germany
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
const collectionName1 = "collection1";
const collectionName2 = "collection2";

var cleanup = function () {
  db._drop(collectionName2);
  db._drop(collectionName1);
};

var setupData = function () {
  const c1 = internal.db._createDocumentCollection(collectionName1);
  const c2 = internal.db._createDocumentCollection(collectionName2);

  c1.save([{ _key: 1 }, { _key: 2 }, { _key: 3 }]);
  c2.save([
    { _key: 1, value: "testValue1" },
    { _key: 2, value: "testValue2" },
    { _key: 3, value: "testValue3" },
  ]);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function undistributeAfterEnumCollRegressionSuite() {
  return {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      cleanup();
      setupData();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      cleanup();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test whether the undistribute-remove-after-enum-coll rule
    /// messes up the execution plan badly.
    ////////////////////////////////////////////////////////////////////////////////
    testUndistributeRule: function () {
      const query = `
        FOR x IN @@c1
          LET gl = DOCUMENT(@c2, x._key)
          LET short = gl.value
          UPDATE x WITH { value: short } IN @@c1
          RETURN NEW.value`;
      const bindVars = {
        "@c1": collectionName1,
        c2: collectionName2,
      };

      var withRule = db._query(query, bindVars);
      var withoutRule = db._query(query, bindVars, {
        optimizer: { rules: ["-undistribute-remove-after-enum-coll"] },
      });

      assertEqual(withoutRule.toArray().sort(), withRule.toArray().sort());
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(undistributeAfterEnumCollRegressionSuite);

return jsunity.done();
