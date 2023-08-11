/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, print */

////////////////////////////////////////////////////////////////////////////////
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const arangodb = require("@arangodb");
const db = arangodb.db;
const dbName = "testOptimizeTopK";
const isEnterprise = require("internal").isEnterprise();

function testOptimizeTopK () {
  return {
    setUpAll: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);
    },
    tearDownAll: function () {
      db._useDatabase('_system');
      try { db._dropDatabase(dbName); } catch (err) { }
    },

    test: function () {
      db._create("c");

      // check correct definition
      db._createView("view1", "arangosearch", {
        links: { "c": { fields: { "f": {}, "a": {}, "x": {} } } },
        optimizeTopK: ["TFIDF(@doc) DESC", "BM25(@doc) DESC"]
      });
      let view_props1 = db._view("view1").properties();
      assertFalse(view_props1.hasOwnProperty("optimizeTopK"));

      // check incorrect definition
      db._createView("view2", "arangosearch", {
        links: { "c": { fields: { "f": {}, "a": {}, "x": {} } } },
        optimizeTopK: ["wrong scorer"]
      });
      let view_props2 = db._view("view2").properties();
      assertFalse(view_props2.hasOwnProperty("optimizeTopK"));

      // check correct definition
      db.c.ensureIndex({ 
        name: "i1", 
        type: "inverted", 
        fields: ["f", "a", "x"], 
        optimizeTopK: ["TFIDF(@doc) DESC", "BM25(@doc) DESC"] 
      });
      let index_props1 = db.c.index("i1");
      assertFalse(index_props1.hasOwnProperty("optimizeTopK"));

      // check incorrect definition
      db.c.ensureIndex({ 
        name: "i2", 
        type: "inverted", 
        fields: ["f", "a", "x", "w"], 
        optimizeTopK: ["wrong scorer"] 
      });
      let index_props2 = db.c.index("i2");
      assertFalse(index_props2.hasOwnProperty("optimizeTopK"));
    }
  };
}

if (!isEnterprise) {
  jsunity.run(testOptimizeTopK);
}
return jsunity.done();
