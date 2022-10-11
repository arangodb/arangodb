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

let jsunity = require("jsunity");
let db = require("@arangodb").db;
let isCluster = require("internal").isCluster();

function noDocumentMaterializationArangoSearchRuleTestSuite () {
  const cn = "UnitTestsCollection";
  const cn1 = "UnitTestsCollection1";
  const vn = "UnitTestsView";
  const svn = "UnitTestsSortView";
  const vvn = "UnitTestsStoredValuesView";
  const systemvn = "UnitTestsSystemFieldsView";
  const edgeIndexCollectionName = "UnitTestsEdgeCollection";
  return {
    setUpAll : function () {
      db._dropView(vn);
      db._dropView(svn);
      db._dropView(vvn);
      db._dropView(systemvn);
      db._drop(cn);
      db._drop(cn1);
      db._drop(edgeIndexCollectionName);

      let c = db._create(cn, { numberOfShards: 3 });
      let c1 = db._create(cn1, { numberOfShards: 3 });
      let edgeCollection = db._createEdgeCollection(edgeIndexCollectionName, { numberOfShards: 3 });

      db._createView(vn, "arangosearch", {
        consolidationIntervalMsec: 5000,
        primarySort: [{"field": "obj.a.a1", "direction": "asc"}, {"field": "obj.b", "direction": "desc"}],
        primarySortCompression: "none",
        storedValues: [["obj.a.a1"], {fields:["obj.c"], compression:"none"}, ["obj.d.d1", "obj.e.e1"], ["obj.f", "obj.g", "obj.h"]],
        links: {
          [cn] : { includeAllFields: true },
          [cn1] : { includeAllFields: true },
        }});

      db._createView(svn, "arangosearch", {
        consolidationIntervalMsec: 5000,
        primarySort: [{"field": "obj.a.a1", "direction": "asc"}, {"field": "obj.b", "direction": "desc"}],
        storedValues: [],
        links: {
          [cn] : { includeAllFields: true },
          [cn1] : { includeAllFields: true }
        }});

      db._createView(vvn, "arangosearch", {
        consolidationIntervalMsec: 5000,
        primarySort: [],
        storedValues: [["obj.a.a1"], ["obj.c"], ["obj.d.d1", "obj.e.e1"], {fields:["obj.f", "obj.g", "obj.h"], compression:"none"}],
        links: {
          [cn] : { includeAllFields: true },
          [cn1] : { includeAllFields: true }
        }});

      db._createView(systemvn, "arangosearch", {
        consolidationIntervalMsec: 5000,
        primarySort: [{"field": "_key", "direction": "asc"}, {"field": "_rev", "direction": "desc"}],
        storedValues: [["_key"], {fields:["_id", "_key"], compression:"none"}, ["_from", "_to", "_id"]],
        links: {
          [cn] : { includeAllFields: true },
          [edgeIndexCollectionName] : { includeAllFields: true }
        }});

      c.save({ _key: 'c0', obj: {a: {a1: 0}, b: {b1: 1}, c: 2, d: {d1: 3}, e: {e1: 4}, f: 5, g: 6, h: 7, j: 8 } });
      c1.save({ _key: 'c_0', obj: {a: {a1: 10}, b: {b1: 11}, c: 12, d: {d1: 13}, e: {e1: 14}, f: 15, g: 16, h: 17, j: 18 } });
      c.save({ _key: 'c1', obj: {a: {a1: 20}, b: {b1: 21}, c: 22, d: {d1: 23}, e: {e1: 24}, f: 25, g: 26, h: 27, j: 28 } });
      c1.save({ _key: 'c_1', obj: {a: {a1: 30}, b: {b1: 31}, c: 32, d: {d1: 33}, e: {e1: 34}, f: 35, g: 36, h: 37, j: 38 } });

      edgeCollection.save({_key: "c_e_0", _from: "testVertices/c0", _to: "testVertices/c1"});
      edgeCollection.save({_key: "c_e_1", _from: "testVertices/c0", _to: "testVertices/c0"});

      // trigger view sync
      db._query("FOR d IN " + vn + " OPTIONS { waitForSync: true } RETURN d");
      db._query("FOR d IN " + svn + " OPTIONS { waitForSync: true } RETURN d");
      db._query("FOR d IN " + vvn + " OPTIONS { waitForSync: true } RETURN d");
      db._query("FOR d IN " + systemvn + " OPTIONS { waitForSync: true } RETURN d");
    },

    tearDownAll : function () {
      try { db._dropView(vn); } catch(e) {}
      try { db._dropView(svn); } catch(e) {}
      try { db._dropView(vvn); } catch(e) {}
      try { db._dropView(systemvn); } catch(e) {}
      try { db._drop(cn); } catch(e) {}
      try { db._drop(cn1); } catch(e) {}
      try { db._drop(edgeIndexCollectionName); } catch(e) {}
    },
    testNotAppliedDueToFullDocumentAccess() {
      let query = "FOR d IN " + vn + " SORT d.obj.j DESC LIMIT 10 RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertFalse(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].hasOwnProperty('noMaterialization'));
    },
    testNotAppliedDueToFalseOptions() {
      let query = "FOR d IN " + vn + " OPTIONS {noMaterialization: false} SORT d.obj.h DESC LIMIT 10 RETURN d.obj.f";
      let plan = AQL_EXPLAIN(query).plan;
      assertFalse(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].hasOwnProperty('noMaterialization'));
    },
    testNotAppliedDueToSubqueryFullDocumentAccess() {
      let query = "FOR d IN " + vn + " SEARCH d.obj.a.a1 IN [0, 10] " +
                  "LET a = NOOPT(d.obj.b.b1) " +
                  "LET e = SUM(FOR c IN " + vn + " LET p = CONCAT(d, c) RETURN p) " +
                  "SORT CONCAT(a, e) LIMIT 10 RETURN d.obj.e.e1";
      let plan = AQL_EXPLAIN(query).plan;
      assertFalse(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].hasOwnProperty('noMaterialization'));
    },
    testNotAppliedDueToAccessToUnknownAttribute() {
      let query = "FOR d IN " + svn
                   // this node will be replaced completely and act as an issue trigger
                  + " SORT d.obj.b ASC LIMIT 10 "
                  // this will fail on third attribute but first two will be replaced and produce the dangling reference
                  + " RETURN {o:d.obj.b, b:d.obj.a.a1, c:d.not_in_sorted }";
      let plan = AQL_EXPLAIN(query).plan;
      assertFalse(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].hasOwnProperty('noMaterialization'));
    },
    testQueryResultsWithSubqueryFullDocumentAccess() {
      let query = "FOR d IN " + vn + " SEARCH d.obj.a.a1 IN [0, 10] " +
                  "LET a = NOOPT(d.obj.b.b1) " +
                  "LET e = SUM(FOR c IN " + vn + " LET p = CONCAT(d, c.obj.d.d1) RETURN p) " +
                  "SORT CONCAT(a, e) LIMIT 10 RETURN d.obj.e.e1";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[1].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set([14, 4]);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc));
        expectedKeys.delete(doc);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsWithSubqueryFullDocumentAccess2() {
      let query = "FOR d IN " + vn + " SEARCH d.obj.a.a1 IN [0, 10] " +
                  "LET a = NOOPT(d.obj.b.b1) " +
                  "LET e = SUM(FOR c IN " + vn + " LET p = CONCAT(d.obj.b, c) RETURN p) " +
                  "SORT CONCAT(a, e) LIMIT 10 RETURN d.obj.e.e1";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set([14, 4]);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc));
        expectedKeys.delete(doc);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsWithoutDocAccess() {
      let query = "FOR d IN " + vn + " RETURN 1";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(4, result.json.length);
      result.json.forEach(function(doc) {
        assertEqual(1, doc);
      });
    },
    testQueryResultsWithSortedAndStoredValueView() {
      let query = "FOR d IN " + vn + " SEARCH d.obj.a.a1 IN [0, 10] SORT d.obj.a.a1 LIMIT 10 RETURN d.obj.e.e1";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set([14, 4]);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc));
        expectedKeys.delete(doc);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsWithScorer() {
      let query = "FOR d IN " + vn + " RETURN BM25(d)";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(4, result.json.length);
    },
    testQueryResultsWithScorerAndStoredValue() {
      let query = "FOR d IN " + vn + " SEARCH d.obj.a.a1 IN [0, 10] RETURN BM25(d)";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
    },
    testQueryResultsWithSortedView() {
      let query = "FOR d IN " + svn + " SEARCH d.obj.a.a1 IN [0, 10] SORT d.obj.a.a1 LIMIT 10 RETURN d.obj.b.b1";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set([11, 1]);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc));
        expectedKeys.delete(doc);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsWithStoredValueView() {
      let query = "FOR d IN " + vvn + " SEARCH d.obj.a.a1 IN [0, 10] SORT d.obj.a.a1 LIMIT 10 RETURN d.obj.e.e1";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set([14, 4]);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc));
        expectedKeys.delete(doc);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsInOuterSubquery() {
      let query = "FOR c IN " + svn + " SEARCH c.obj.a.a1 IN [0, 10] SORT c.obj.a.a1 DESC LIMIT 10 " +
                  "FOR d IN " + vn + " SEARCH d.obj.a.a1 IN [20, 30] RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(4, result.json.length);
      let expected = new Set(['c1', 'c_1']);
      result.json.forEach(function(doc) {
        assertTrue(expected.has(doc._key));
      });
    },
    testQueryResultsWithSystemFields() {
      let query = "FOR d IN " + systemvn + " LET a = d._key SORT a DESC, d._rev LIMIT 2 RETURN d._id";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set([cn + "/c0", cn + "/c1"]);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc));
        expectedKeys.delete(doc);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsWithSystemEdgeFields() {
      let query = "FOR d IN " + systemvn + " FILTER d._from == 'testVertices/c0' SORT NOOPT(d._from) DESC LIMIT 10 RETURN d._to";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set(["testVertices/c1", "testVertices/c0"]);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc));
        expectedKeys.delete(doc);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsWithTrueOptions() {
      let query = "FOR d IN " + vn + " OPTIONS {noMaterialization: true} SORT d.obj.h DESC LIMIT 2 RETURN d.obj.f";
      let plan = AQL_EXPLAIN(query).plan;
      assertTrue(plan.nodes.filter(obj => {
        return obj.type === "EnumerateViewNode";
      })[0].noMaterialization);
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set([25, 35]);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc));
        expectedKeys.delete(doc);
      });
      assertEqual(0, expectedKeys.size);
    }
  };
}

jsunity.run(noDocumentMaterializationArangoSearchRuleTestSuite);

return jsunity.done();
