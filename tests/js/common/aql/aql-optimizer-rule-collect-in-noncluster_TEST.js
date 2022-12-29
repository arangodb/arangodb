/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for COLLECT w/ COUNT
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = require("@arangodb").db;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;

function optimizerCollectInClusterSuite(isSearchAlias) {
  let c;
  let v;
  let v_sa;

  return {
    setUpAll: function () {
      // db._createDatabase("Test", {sharding: "single"});
      // db._useDatabase("Test");
      db._dropView("UnitTestViewSorted");
      db._dropView("UnitTestViewSortedSA");
      db._drop("UnitTestsCollection");

      c = db._create("UnitTestsCollection", { numberOfShards: 3 });

      let indexMeta = {};
      indexMeta = {
        type: "inverted",
        name: "inverted",
        includeAllFields: true,
        primarySort: { fields: [{ "field": "value", "direction": "asc" }] },
        fields: [
          { "name": "value_nested", "nested": [{ "name": "nested_1", "nested": [{ "name": "nested_2" }] }] },
          "value",
          "group"
        ]
      };

      let i = c.ensureIndex(indexMeta);
      v_sa = db._createView("UnitTestViewSortedSA", "search-alias", {
        indexes: [{ collection: "UnitTestsCollection", index: i.name }]
      });

      let viewMeta = {};
      viewMeta = {
        primarySort: [{ "field": "value", "direction": "asc" }],
        links: {
          UnitTestsCollection: {
            includeAllFields: false,
            fields: {
              value: { analyzers: ["identity"] },
              group: { analyzers: ["identity"] },
              "value_nested": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } }
            }
          }
        }
      };
      v = db._createView("UnitTestViewSorted", "arangosearch", viewMeta);

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ group: "test" + (i % 10), value: i, "value_nested": [{ "nested_1": [{ "nested_2": `foo${i}` }] }] });
      }
      c.insert(docs);
      print("count = ", c.count());

      assertEqual(db._query(`FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.value >= 0 COLLECT WITH COUNT INTO C RETURN C`).toArray()[0], 1000);
      assertEqual(db._query("FOR d IN UnitTestViewSorted OPTIONS {waitForSync:true} COLLECT WITH COUNT INTO C RETURN C").toArray()[0], 1000);
    },

    tearDownAll: function () {
      db._dropView("UnitTestViewSorted");
      db._drop("UnitTestsCollection");
      db._dropView("UnitTestViewSortedSA");

    },

    testDistinct: function () {
      let query = "FOR doc IN " + c.name() + " SORT doc.value RETURN DISTINCT doc.value";
      let results = db._query(query).toArray();
      // print(JSON.stringify(results))
      // print("\n\n\n\n")
      assertEqual(1000, results.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results[i]);
      }

      {
        let arangosearchQuery = `FOR doc IN UnitTestViewSorted SEARCH doc.value >= 0 SORT doc.value RETURN DISTINCT doc.value`;

        let arangosearchResults = db._query(arangosearchQuery).toArray();
        // print(JSON.stringify(arangosearchResults))
        // print("\n\n\n\n")

        assertEqual(1000, arangosearchResults.length);
        for (let i = 0; i < 1000; ++i) {
          assertEqual(i, arangosearchResults[i]);
        }
      }

      {
        let arangosearchQuery = `FOR doc IN UnitTestViewSortedSA SEARCH doc.value >= 0 SORT doc.value RETURN DISTINCT doc.value`;

        let arangosearchResults = db._query(arangosearchQuery).toArray();
        // print(JSON.stringify(arangosearchResults))
        // print("\n\n\n\n")
        assertEqual(1000, arangosearchResults.length);
        for (let i = 0; i < 1000; ++i) {
          assertEqual(i, arangosearchResults[i]);
        }
      }

      {
        let indexQuery = `FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
        FILTER doc.value >= 0
        SORT doc.value RETURN DISTINCT doc.value`;

        // let indexQuery = `FOR doc IN UnitTestsCollection OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
        // FILTER doc.value_nested[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]]
        // SORT doc.value RETURN DISTINCT doc.value`;

        db._explain(indexQuery);
        let indexResults = db._query(indexQuery).toArray();

        // let indexResults = db._query(indexQuery, {}, {optimizer:{rules:["-use-index-for-sort"]}}).toArray();
        print(JSON.stringify(indexResults))
        print("\n\n\n\n")

        assertEqual(1000, indexResults.length);
        for (let i = 0; i < 1000; ++i) {
          assertEqual(i, indexResults[i]);
        }
      }
    },
  };
}

function optimizerCollectInClusterSearchAliasSuite() {
  let suite = {};
  deriveTestSuite(
    optimizerCollectInClusterSuite(true),
    suite,
    "_search-alias"
  );
  return suite;
}


function optimizerCollectInClusterSingleShardSearchAliasSuite() {
  let suite = {};
  deriveTestSuite(
    optimizerCollectInClusterSingleShardSuite(true),
    suite,
    "_search-alias"
  );
  return suite;
}


jsunity.run(optimizerCollectInClusterSearchAliasSuite);
return jsunity.done();
