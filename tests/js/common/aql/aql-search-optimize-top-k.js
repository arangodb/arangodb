/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, print */

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
const arangodb = require("@arangodb");
const db = arangodb.db;
const isEnterprise = require("internal").isEnterprise();
const dbName = "testOptimizeTopK";

function testOptimizeTopK() {
  const names = ["v_alias_tfidf", "v_alias_bm25", "v_search_tfidf", "v_search_bm25"];

  return {
    setUpAll: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      let docs = [];
      for (let i = 0; i < 600; ++i) {
        docs.push({ f: "a", c: "b", x: "c" });
        docs.push({ f: "f", c: "c", x: "x" });
      }

      db._create("c");

      for (let i = 0; i < 50; ++i) {
        db.c.insert(docs);
      }

      db._createView("v_search_tfidf", "arangosearch", {
        links: { "c": { fields: { "f": {}, "a": {}, "x": {} } } },
        optimizeTopK: ["TFIDF(@doc) DESC"]
      });

      db.c.ensureIndex({ name: "i_bm25", type: "inverted", fields: ["f", "a", "x"], optimizeTopK: ["BM25(@doc) DESC"] });
      db._createView("v_alias_bm25", "search-alias", { indexes: [{ collection: "c", index: "i_bm25" }] });

      db.c.ensureIndex({ name: "i_tfidf", type: "inverted", fields: ["f", "a", "x"], optimizeTopK: ["TFIDF(@doc) DESC"] });
      db._createView("v_alias_tfidf", "search-alias", { indexes: [{ collection: "c", index: "i_tfidf" }] });

      db._createView("v_search_bm25", "arangosearch", {
        links: { "c": { fields: { "f": {}, "a": {}, "x": {} } } },
        optimizeTopK: ["BM25(@doc) DESC"]
      });

      for (let i = 0; i < 50; ++i) {
        db.c.insert(docs);
      }

      // testWithoutScore
      for (const v of names) {
        const query = "FOR d IN " + v + " SEARCH d.f == 'a' OPTIONS {waitForSync:true} LIMIT 10 RETURN d";
        let res = db._query(query).toArray();
        assertEqual(res.length, 10);
      }
    },

    tearDownAll: function () {
      db._useDatabase('_system');
      try { db._dropDatabase(dbName); } catch (err) { }
    },

    testWithTFIDF: function () {
      for (const v of names) {
        const query = "FOR d IN " + v + " SEARCH d.f == 'a' SORT TFIDF(d) LIMIT 10 RETURN d";
        let res = db._query(query).toArray();
        assertEqual(res.length, 10);
      }
    },

    testWithBM25: function () {
      for (const v of names) {
        const query = "FOR d IN " + v + " SEARCH d.f == 'a' SORT BM25(d) LIMIT 10 RETURN d";
        let res = db._query(query).toArray();
        assertEqual(res.length, 10);
      }
    },

    testNegativeScenarios: function () {

      // Malformed scorers definitions
      {
        let scorers = [];
        for (let i = 0; i < 65; ++i) {
          scorers.push(`bm25(@doc, ${i}, ${i}) DESC`);
        }

        [
          scorers,
          ["bm25(@doc) DESC", 2, "tfidf(@doc) DESC"],
          ["bm25(@doc) ASC"],
          ["bm25(@doc, 'abc') DESC"],
          ["bm25(@doc, true, true) DESC"],
          ["bm25(@doc, 1,1,1) DESC"],

          ["tfidf(@doc) ASC"],
          ["tfidf(@doc, 1) DESC"],
          ["tfidf(@doc, true, true) DESC"],
          ["abc"],
        ].forEach(t => {
          try {
            db._createView("fail", "arangosearch", {
              links: { "c": { fields: { "f": {} } } },
              optimizeTopK: t
            });
            assertTrue(false);
          } catch (err) { }

          try {
            db.c.ensureIndex({ name: "fail", type: "inverted", fields: ["f"], optimizeTopK: t });
            assertTrue(false);
          } catch (err) { }
        });

        assertEqual(db._view("fail"), null);
      }

      // Try to cover with search-alias different optimizeTopK values
      {
        db.c.ensureIndex({ name: "i1", type: "inverted", fields: ["f", "a"], optimizeTopK: ["tfidf(@doc) desc"] });
        db.c.ensureIndex({ name: "i2", type: "inverted", fields: ["f", "b"], optimizeTopK: ["bm25(@doc) desc"] });

        try {
          db._createView("fail", "search-alias", { indexes: [{ collection: "c", index: "i1" }, { collection: "c", index: "i2" }] });
          assertTrue(false);
        } catch (err) { }

        assertEqual(db._view("fail"), null);
      }

      // try to update arangosearch view. No changes should apply
      {
        db._view("v_search_tfidf").properties({optimizeTopK: ["BM25(@doc) DESC"]}, true);
        assertEqual(db._view("v_search_tfidf").properties()["optimizeTopK"], ["TFIDF(@doc) DESC"]);
      }
    }
  };
}

if (isEnterprise) {
  jsunity.run(testOptimizeTopK);
}

return jsunity.done();
