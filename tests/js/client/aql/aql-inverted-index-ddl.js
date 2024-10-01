/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertFalse, assertTrue */

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
/// @author Andrei Lobov
// //////////////////////////////////////////////////////////////////////////////

let arangodb = require("@arangodb");
let db = arangodb.db;
let internal = require("internal");
let jsunity = require("jsunity");
const dbName = "InvertedIndexDdl";
const colName = "InvertedIndexDdl";
const isEnterprise = internal.isEnterprise();

function testSuite() {
  return {
    setUp: function() {
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._create("c0");
    },
    tearDown: function() {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },
    testCachedColumns : function() {
      db.c0.ensureIndex({
          "type": "inverted",
          "name": "c0_cached",
          "fields": [
            {"name":"a", "cache":true}
          ],
          "primaryKeyCache":true,
          "primarySort":{"cache":true, "fields":[{"field":"a", "direction":"asc"}]},
          "storedValues" : [{
            "fields" : [
              "a"
            ],
            "compression" : "lz4",
            "cache" : true
          }],
          "includeAllFields": true,
          "analyzer": "identity"
      });
      let properties = db.c0.getIndexes().find(function (idx) { return idx.name === "c0_cached";});
      if (isEnterprise) {
        assertTrue(properties.fields[0].cache);
        assertTrue(properties.primaryKeyCache);
        assertTrue(properties.storedValues[0].cache);
      } else {
        assertFalse(properties.fields[0].hasOwnProperty("cache"));
        assertFalse(properties.hasOwnProperty("primaryKeyCache"));
        assertFalse(properties.storedValues[0].hasOwnProperty("cache"));
      }
    }
};}

jsunity.run(testSuite);
return jsunity.done();
