/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");
var assertQueryError = helper.assertQueryError;
const isCluster = require("@arangodb/cluster").isCluster();
const isEnterprise = require("internal").isEnterprise();
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;

function viewFiltersMerging(isSearchAlias) {
  return {
    setUpAll: function () {
      db._dropView("UnitTestView");
      db._dropView("UnitTestViewSorted");
      db._drop("UnitTestsCollection");
      let c = db._create("UnitTestsCollection", {numberOfShards: 3});

      let docs = [];
      for (let i = 0; i < 50; ++i) {
        docs.push({value: "footest" + i, count: i});
        docs.push({ name_1: i.toString(), "value_nested": [{ "nested_1": [{ "nested_2": "foo123"}]}]});
      }
      c.insert(docs);
      if (isSearchAlias) {
        let c = db._collection("UnitTestsCollection");
        let indexMeta = {};
        if (isEnterprise) {
          indexMeta = {
            type: "inverted",
            name: "inverted",
            fields: ["value", "count", {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}]
          };
        } else {
          indexMeta = {
            type: "inverted",
            name: "inverted",
            fields: ["value", "count"]
          };
        }
        let i = c.ensureIndex(indexMeta);
        db._createView("UnitTestView", "search-alias", {indexes: [{collection: "UnitTestsCollection", index: i.name}]});
      } else {
        let viewMeta = {};
        if (isEnterprise) {
          viewMeta = {
            links: {
              UnitTestsCollection: {
                includeAllFields: false,
                fields: {
                  value: {
                    analyzers: ["identity"]
                  },
                  count: {
                    analyzers: ["identity"]
                  },
                  value_nested: {
                    "nested": { "nested_1": {"nested": {"nested_2": {}}}}
                  }
                }
              }
            }
          };
        } else {
          viewMeta = {
            links: {
              UnitTestsCollection: {
                includeAllFields: false,
                fields: {
                  value: {
                    analyzers: ["identity"]
                  },
                  count: {
                    analyzers: ["identity"]
                  },
                  value_nested: {}
                }
              }
            }
          };
        }
        db._createView("UnitTestView", "arangosearch", viewMeta);
      }
      // sync the views
      db._query("FOR d IN UnitTestView OPTIONS {waitForSync:true} LIMIT 1 RETURN d");
    },

    tearDownAll: function () {
      db._dropView("UnitTestView");
      db._drop("UnitTestsCollection");
    },

    testMergeSimple() {
      let res = db._query("FOR d IN UnitTestView SEARCH " +
        "LEVENSHTEIN_MATCH(d.value, 'footest', 1, false) " +
        "AND STARTS_WITH(d.value, 'footest') RETURN d").toArray();
      assertEqual(10, res.length);

      if(isSearchAlias) {
        res = db._query(`FOR d IN UnitTestsCollection 
          OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true, filterOptimization: 0} FILTER
          LEVENSHTEIN_MATCH(d.value, 'footest', 1, false) AND
          STARTS_WITH(d.value, 'footest') RETURN d`).toArray();
        assertEqual(10, res.length);
      }
    },
    testMergeDisabled() {
      let res = db._query("FOR d IN UnitTestView SEARCH " +
        "LEVENSHTEIN_MATCH(d.value, 'footest', 2, false) " +
        "AND STARTS_WITH(d.value, 'footest') RETURN d").toArray();
      assertEqual(50, res.length);

      if (isSearchAlias) {
        let res = db._query(`FOR d IN UnitTestsCollection 
          OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true, filterOptimization: 0} FILTER
          LEVENSHTEIN_MATCH(d.value, 'footest', 2, false) AND
          STARTS_WITH(d.value, 'footest') RETURN d`).toArray();
        assertEqual(50, res.length);
      }
    },
  };
}

function arangoSearchFiltersMerging() {
  let suite = {};
  deriveTestSuite(
    viewFiltersMerging(false),
    suite,
    "_arangosearch"
  );
  return suite;
}

function searchAliasFiltersMerging() {
  let suite = {};
  deriveTestSuite(
    viewFiltersMerging(true),
    suite,
    "_search-alias"
  );
  return suite;
}

jsunity.run(arangoSearchFiltersMerging);
jsunity.run(searchAliasFiltersMerging);

return jsunity.done();

