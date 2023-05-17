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

function testOptimizeTopK() {
  const names = ["v_alias_tfidf", "v_alias_bm25", "v_search_tfidf", "v_search_bm25"];

  return {
    setUpAll: function () {
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({f: "a"});
      }

      db._create("c");

      for (let i = 0; i < 50; ++i) {
        db.c.insert(docs);
      }

      db._createView("v_search_tfidf", "arangosearch", {
        links: {"c": {fields: {"f": {}}}},
        optimizeTopK: ["TFIDF(@doc) DESC"]
      });

      db.c.ensureIndex({name: "i_bm25", type: "inverted", fields: ["f"], optimizeTopK: ["BM25(@doc) DESC"]});
      db._createView("v_alias_bm25", "search-alias", {indexes: [{collection: "c", index: "i_bm25"}]});

      db.c.ensureIndex({name: "i_tfidf", type: "inverted", fields: ["f"], optimizeTopK: ["TFIDF(@doc) DESC"]});
      db._createView("v_alias_tfidf", "search-alias", {indexes: [{collection: "c", index: "i_tfidf"}]});

      db._createView("v_search_bm25", "arangosearch", {
        links: {"c": {fields: {"f": {}}}},
        optimizeTopK: ["BM25(@doc) DESC"]
      });

      for (let i = 0; i < 50; ++i) {
        db.c.insert(docs);
      }

      // testWithoutScore
      for (const v of names) {
        const query = "FOR d IN " + v + " SEARCH d.f == 'a' OPTIONS {waitForSync:true} LIMIT 10 RETURN d";
        print(query);
        let res = db._query(query).toArray();
        assertEqual(res.length, 10);
      }
    },

    tearDownAll: function () {
      for (const v of names) {
        db._dropView(v);
      }
      db._drop("c");
    },

    testWithTFIDF: function () {
      for (const v of names) {
        const query = "FOR d IN " + v + " SEARCH d.f == 'a' SORT TFIDF(d) LIMIT 10 RETURN d";
        print(query);
        let res = db._query(query).toArray();
        assertEqual(res.length, 10);
      }
    },

    testWithBM25: function () {
      for (const v of names) {
        const query = "FOR d IN " + v + " SEARCH d.f == 'a' SORT BM25(d) LIMIT 10 RETURN d";
        print(query);
        let res = db._query(query).toArray();
        assertEqual(res.length, 10);
      }
    },
  };
}

if (isEnterprise) {
  jsunity.run(testOptimizeTopK);
}

return jsunity.done();
