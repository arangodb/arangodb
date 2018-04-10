/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertException */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSatelliteTestSuite () {
  const collName = "UnitTestsAhuacatlSatellite";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      internal.db._drop(collName);
      let coll = internal.db._create(collName, {replicationFactor: "satellite"});

      for (let i = 1; i <= 100; ++i) {
        coll.save({"value": i});
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      internal.db._drop(collName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief regression test for internal issue #2237
////////////////////////////////////////////////////////////////////////////////

    testAqlAgainstSatellite: function () {
      // just any simple query against a collection with
      // {replicationFactor: "satellite"}
      const query = "FOR x IN @@collection SORT x.value LIMIT @limit RETURN x.value";
      const bind = {
        "@collection": collName,
        "limit": 1,
      };
      const expected = [1];
      const actual = getQueryResults(query, bind);

      assertEqual(expected, actual);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSatelliteTestSuite);

return jsunity.done();

