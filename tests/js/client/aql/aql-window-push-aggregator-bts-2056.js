/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const {db} = require("@arangodb");

const database = "WindowPushAggTestDb";
const collection = "c";

function queryWindowPushAggregation() {

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
      db._create(collection);
      db._query(`FOR i IN 1..100 INSERT {x: i} INTO ${collection}`);
    },
    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testUnboundedWindow: function () {
      const result = db._query(`FOR doc IN ${collection} WINDOW { preceding: "unbounded", following: 0}
        AGGREGATE v = PUSH(doc.x) RETURN v`).toArray();

      assertEqual(result.length, 100);
      for (let k = 0; k < 100; k++) {
        const row = result[k];
        assertEqual(row.length, k + 1);
        for (let j = 0; j < k; j++) {
          assertEqual(row[j], j + 1);
        }
      }
    }
  };
}

jsunity.run(queryWindowPushAggregation);
return jsunity.done();
