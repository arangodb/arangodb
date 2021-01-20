/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for late document materialization arangosearch rule
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function() {
  var db = require("@arangodb").db;
  const analyzers = require("@arangodb/analyzers");

  return function (isCluster, newDbName, dbArgs) {
    const ruleName = "late-document-materialization-arangosearch";
    const cn = "UnitTestsCollection";
    const cn1 = "UnitTestsCollection1";
    const vn = "UnitTestsView";
    return {
      setUpAll : function () {
        db._useDatabase("_system");
        try { db._dropDatabase(newDbName); } catch {}
        db._createDatabase(newDbName, dbArgs);
        db._useDatabase(newDbName);

        let c = db._create(cn, { numberOfShards: 3 });
        let c1 = db._create(cn1, { numberOfShards: 3 });

        db._createView(vn, "arangosearch", {
          consolidationIntervalMsec: 5000,
          primarySort: [{"field": "obj.a.a1", "direction": "asc"}, {"field": "obj.b", "direction": "desc"}],
          primarySortCompression: "none",
          storedValues: [["obj.a.a1"], {fields:["obj.c"], compression:"none"}, {fields:["obj.d.d1", "obj.e.e1"], compression:"lz4"}, ["obj.f", "obj.g", "obj.h"]],
          links: {
            [cn] : { includeAllFields: true },
            [cn1] : { includeAllFields: true }
          }});

        c.save({ _key: 'c0', obj: {a: {a1: 0}, b: {b1: 1}, c: 2, d: {d1: 3}, e: {e1: 4}, f: 5, g: 6, h: 7, j: 8 } });
        c1.save({ _key: 'c_0', obj: {a: {a1: 10}, b: {b1: 11}, c: 12, d: {d1: 13}, e: {e1: 14}, f: 15, g: 16, h: 17, j: 18 } });
        c.save({ _key: 'c1', obj: {a: {a1: 20}, b: {b1: 21}, c: 22, d: {d1: 23}, e: {e1: 24}, f: 25, g: 26, h: 27, j: 28 } });
        c1.save({ _key: 'c_1', obj: {a: {a1: 30}, b: {b1: 31}, c: 32, d: {d1: 33}, e: {e1: 34}, f: 35, g: 36, h: 37, j: 38 } });

        // trigger view sync
        db._query("FOR d IN " + vn + " OPTIONS { waitForSync: true } RETURN d");
      },

      tearDownAll : function () {
        db._useDatabase("_system");
        db._dropDatabase(newDbName);
      },
      testNotAppliedDueToNotStored() {
        let query = "FOR d IN " + vn + " SORT d.obj.j DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNotStored2() {
        let query = "FOR d IN " + vn + " SORT d.obj.a.a1, d.obj.j DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNotStored3() {
        let query = "FOR d IN " + vn + " SORT d.obj.e DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testQueryResultsWithSortFields() {
        let query = "FOR d IN " + vn + " SORT d.obj.a.a1 DESC, d.obj.b.b1 LIMIT 2 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expectedKeys = new Set(['c1', 'c_1']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithStoredFields() {
        let query = "FOR d IN " + vn + " SORT d.obj.d.d1, d.obj.f DESC LIMIT 2 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expectedKeys = new Set(['c0', 'c_0']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithMixedSortAndStoredFields() {
        let query = "FOR d IN " + vn + " SORT d.obj.d.d1, d.obj.b.b1 DESC LIMIT 2 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expectedKeys = new Set(['c0', 'c_0']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithVariable() {
        let query = "FOR d IN " + vn + " LET c = NOOPT(d.obj.d.d1) SORT c LIMIT 2 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expectedKeys = new Set(['c0', 'c_0']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithSEARCH() {
        let query = "FOR d IN " + vn + " SEARCH d.obj.h in [7, 27] SORT d.obj.e.e1 LIMIT 2 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expectedKeys = new Set(['c0', 'c1']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithTwoDifferentAccessesToCommonFieldOfColumn() {
        let query = "FOR d IN " + vn + " LET c = d.obj.b SORT d.obj.b.b1, c LIMIT 2 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        assertEqual(1, plan.nodes.filter(obj => {
          return obj.type === "EnumerateViewNode";
        })[0].viewValuesVars.length);
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expectedKeys = new Set(['c0', 'c_0']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithTwoDifferentAccessesToSingleColumn() {
        let query = "FOR d IN " + vn + " FILTER d.obj.d.d1 != d.obj.e.e1 SORT d.obj.d.d1 LIMIT 2 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        assertEqual(1, plan.nodes.filter(obj => {
          return obj.type === "EnumerateViewNode";
        })[0].viewValuesVars.length);
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expectedKeys = new Set(['c0', 'c_0']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryLateMaterializationForSortedView() {
        let query = "FOR d IN " + vn + " SEARCH d.obj.f != 15 SORT d.obj.a.a1 ASC LIMIT 10 SORT d.obj.c LIMIT 2  RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        assertEqual(2, plan.nodes.filter(obj => {
          return obj.type === "EnumerateViewNode";
        })[0].viewValuesVars.length);
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expectedKeys = new Set(['c0', 'c1']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      }
    };
  };
}());
