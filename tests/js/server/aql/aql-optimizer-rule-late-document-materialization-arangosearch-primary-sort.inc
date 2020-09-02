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
    const svn = "SortedTestsView";
    return {
      setUpAll : function () {
        db._useDatabase("_system");
        try { db._dropDatabase(newDbName); } catch {}
        db._createDatabase(newDbName, dbArgs);
        db._useDatabase(newDbName);

        let c = db._create(cn, { numberOfShards: 3 });
        let c2 = db._create(cn1, { numberOfShards: 3 });

        db._createView(vn, "arangosearch", {
          links: {
            [cn] : { includeAllFields: true, analyzers : [ "identity" ],
                     fields : { str : { "analyzers" : [ "text_en" ] }}, },
            [cn1] : { includeAllFields: true, analyzers : [ "identity" ],
                      fields : { str : { "analyzers" : [ "text_en" ] }}}
          }});
        db._createView(svn, "arangosearch", {
          consolidationIntervalMsec: 5000,
          primarySort: [{"field": "value", "direction": "asc"}, {"field": "foo", "direction": "desc"}],
          links: {
            [cn] : { includeAllFields: true },
            [cn1] : { includeAllFields: true }
          }});

        c.save({ _key: 'c0',  str: 'cat', foo: 'foo0', value: 0 });
        c2.save({_key: 'c_0', str: 'cat', foo: 'foo1', value: 10 });
        c.save({ _key: 'c1',  str: 'cat', foo: 'foo2', value: 1 });
        c2.save({_key: 'c_1', str: 'cat', foo: 'foo3', value: 11 });
        c.save({ _key: 'c2',  str: 'cat', foo: 'foo4', value: 2 });
        c2.save({_key: 'c_2', str: 'cat', foo: 'foo5', value: 12 });
        c.save({ _key: 'c3',  str: 'cat', foo: 'foo6', value: 3 });
        c2.save({_key: 'c_3', str: 'cat', foo: 'foo7', value: 13 });

        // trigger view sync
        db._query("FOR d IN " + vn + " OPTIONS { waitForSync: true } RETURN d");
        db._query("FOR d IN " + svn + " OPTIONS { waitForSync: true } RETURN d");
      },

      tearDownAll : function () {
        db._useDatabase("_system");
        db._dropDatabase(newDbName);
      },
      testNotAppliedDueToNoPrimarySort() {
        let query = "FOR d IN " + vn + " SEARCH d.value IN [1, 2] SORT d.foo DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToSortElimination() {
        let query = "FOR d IN " + svn + " LET c = d.value + RAND() SORT d.value LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNoSort() {
        let query = "FOR d IN " + svn + " LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToUsedInInnerSort() {
        let query = "FOR d IN " + svn + " SORT NOOPT(d.str) SORT d.value DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNoLimit() {
        let query = "FOR d IN " + svn + " SORT d.value DESC RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToUpdateDoc() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] UPDATE d IN " + cn + " SORT d.foo DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToSubqueryWithDocumentAccess() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] " +
                    "LET a = NOOPT(d.foo) " +
                    "LET e = SUM(FOR c IN " + vn + " LET p = CONCAT(d, c.foo) RETURN p) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToSubqueryWithReturnDocument() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] " +
                    "LET a = NOOPT(d.foo) " +
                    "LET e = SUM(FOR c IN " + vn + " LET p = NOOPT(CONCAT(d.foo, c.foo)) RETURN d) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testQueryResultsWithSubqueryWithDocumentAccessByAttribute() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] " +
                    "LET a = NOOPT(d.foo) " +
                    "LET e = SUM(FOR c IN " + vn + " LET p = CONCAT(d.foo, c.foo) RETURN p) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        if (!isCluster) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query);
          assertEqual(4, result.json.length);
          let expectedKeys = new Set(['c1', 'c2', 'c_1', 'c_2']);
          result.json.forEach(function(doc) {
            assertTrue(expectedKeys.has(doc._key));
            expectedKeys.delete(doc._key);
          });
          assertEqual(0, expectedKeys.size);
        } else {
          // on cluster this will not be applied as remote node placed before sort node
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      },
      testQueryResultsWithInnerSubqueriesWithDocumentAccessByAttribute() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] " +
                    "LET a = NOOPT(d.foo) " +
                    "LET e = SUM(FOR c IN " + vn + " LET p = CONCAT(d.foo, c.foo) " +
                      "LET f = SUM(FOR k IN " + svn + " LET r = CONCAT(c.foo, k.foo) RETURN CONCAT(c.value, r)) RETURN CONCAT(d.value, p, f)) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        if (!isCluster) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query);
          assertEqual(4, result.json.length);
          let expectedKeys = new Set(['c1', 'c2', 'c_1', 'c_2']);
          result.json.forEach(function(doc) {
            assertTrue(expectedKeys.has(doc._key));
            expectedKeys.delete(doc._key);
          });
          assertEqual(0, expectedKeys.size);
        } else {
          // on cluster this will not be applied as remote node placed before sort node
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      },
      testQueryResultsWithSubqueryWithoutDocumentAccess() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] " +
                    "LET a = NOOPT(d.foo) " +
                    "LET e = SUM(FOR c IN " + vn + " LET p = CONCAT(c.foo, c.foo) RETURN p) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        if (!isCluster) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query);
          assertEqual(4, result.json.length);
          let expectedKeys = new Set(['c1', 'c2', 'c_1', 'c_2']);
          result.json.forEach(function(doc) {
            assertTrue(expectedKeys.has(doc._key));
            expectedKeys.delete(doc._key);
          });
          assertEqual(0, expectedKeys.size);
        } else {
          // on cluster this will not be applied as remote node placed before sort node
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      },
      testQueryResultsWithFilter() {
        let query = "FOR d IN " + svn + " FILTER d.value IN [1, 2] SORT d.foo DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expectedKeys = new Set(['c1', 'c2']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithTwoSorts() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] " +
                    "SORT d.value DESC LET c = BM25(d) * 2 SORT CONCAT(BM25(d), c) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(4, result.json.length);
        let expectedKeys = new Set(['c1', 'c2', 'c_1', 'c_2']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithRandomSort() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] SORT RAND(), d.value DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(4, result.json.length);
        let expectedKeys = new Set(['c1', 'c2', 'c_1', 'c_2']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithTwoSortFields() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC, d.foo LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(4, result.json.length);
        let expectedKeys = new Set(['c1', 'c2', 'c_1', 'c_2']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithMultipleCollections() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(4, result.json.length);
        let expectedKeys = new Set(['c1', 'c2', 'c_1', 'c_2']);
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithMultipleCollectionsWithAfterSort() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 10 SORT NOOPT(d.value) ASC RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(4, result.json.length);
        let expectedKeys = new Set(['c1', 'c2', 'c_1', 'c_2']);
        let currentValue  = 0;
        result.json.forEach(function(doc) {
          assertTrue(expectedKeys.has(doc._key));
          expectedKeys.delete(doc._key);
          // check after sort asc order
          assertTrue(currentValue < doc.value);
          currentValue = doc.value;
        });
        assertEqual(0, expectedKeys.size);
      },
      testQueryResultsWithMultipleCollectionsWithMultiSort() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] " +
                    "SORT d.value DESC LIMIT 10 SORT TFIDF(d) DESC LIMIT 4 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        let nodeDependency = null;
        plan.nodes.forEach(function(node) {
          if (node.type === "MaterializeNode") {
            // there should be no materializer before (e.g. double materialization)
            assertFalse(materializeNodeFound);
            materializeNodeFound = true;
            // the other sort node should be limited but not have a materializer
            // d.value node on single and TFIDF on cluster as for cluster
            // only first sort will be on DBServers
            assertEqual(nodeDependency.limit, isCluster ? 0 : 4);
          }
          nodeDependency = node; // as we walk the plan this will be next node dependency
        });
        // materilizer should be there
        assertTrue(materializeNodeFound);
        let result = AQL_EXECUTE(query);
        assertEqual(4, result.json.length);
        // should be sorted by increasing cat frequency
        let expectedKeys = ['c_2', 'c_1', 'c2', 'c1'];
        result.json.forEach(function(doc) {
          assertEqual(expectedKeys[0], doc._key);
          expectedKeys.shift();
        });
        assertEqual(0, expectedKeys.length);
      },
      testQueryResultsWithMultipleCollectionsAfterCalc() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 10 LET c = CONCAT(NOOPT(d._key), '-C') RETURN c";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(4, result.json.length);
        let expected = new Set(['c1-C', 'c2-C', 'c_1-C', 'c_2-C']);
        result.json.forEach(function(doc) {
          assertTrue(expected.has(doc));
          expected.delete(doc);
        });
        assertEqual(0, expected.size);
      },
      testQueryResultsSkipSome() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 3, 1 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(1, result.json.length);
        assertEqual(result.json[0]._key, 'c1');
      },
      testQueryResultsSkipAll() {
        let query = "FOR d IN " + svn + " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 5, 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(0, result.json.length);
      },
      testQueryResultsInSubquery() {
        let query = "FOR c IN " + vn + " SEARCH c.value == 1 " +
                    "FOR d IN " + svn + " SEARCH d.value IN [c.value, c.value + 1] SORT d.value DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expected = new Set(['c1', 'c2']);
        result.json.forEach(function(doc) {
          assertTrue(expected.has(doc._key));
          expected.delete(doc._key);
        });
        assertEqual(0, expected.size);
      },
      testQueryResultsInOuterSubquery() {
        let query = "FOR c IN " + svn + " SEARCH c.value == 1 SORT c.value DESC LIMIT 10 " +
                    "FOR d IN " + vn + " SEARCH d.value IN [c.value, c.value + 1] RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expected = new Set(['c1', 'c2']);
        result.json.forEach(function(doc) {
          assertTrue(expected.has(doc._key));
          expected.delete(doc._key);
        });
        assertEqual(0, expected.size);
      },
      testQueryResultsMultipleLimits() {
        let query = "FOR d IN " + svn + " SEARCH d.value > 5 SORT d.value DESC " +
                    "LIMIT 1, 5 SORT d.foo LIMIT 1, 3 SORT NOOPT(d.str) DESC " +
                    "LIMIT 1, 1 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        let nodeDependency = null;
        // sort by d.value node`s limit must be appended with materializer (identified by limit value = 1)
        // as last SORT needs materialized document
        // and SORT by d.foo is not lowest possible variant
        // However in cluster only first sort suitable, as later sorts depend
        // on all db servers results and performed on coordinator
        plan.nodes.forEach(function(node) {
          if (node.type === "MaterializeNode") {
            assertFalse(materializeNodeFound); // no double materialization
            assertEqual(nodeDependency.limit, isCluster ? 0 : 3);
            materializeNodeFound = true;
          }
          nodeDependency = node;
        });
        assertTrue(materializeNodeFound);
      },
      testQueryResultsMultipleLimits2() {
        // almost the same as testQueryResultsMultipleLimits but without last sort - this
        // will not create addition variable for sort
        // value but it should not affect results especially on cluster!
        let query = " FOR d IN " + svn  + " SEARCH d.value > 5 SORT d.value DESC " +
                    " LIMIT 1, 5 SORT d.foo LIMIT 1, 3 " +
                    " RETURN d ";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        // sort by d.foo node`s limit must be appended with materializer (identified by limit value = 3)
        // as SORT by d.value is not lowest possible variant
        // However in cluster only first sort suitable, as later sorts depend
        // on all db servers results and performed on coordinator
        let nodeDependency = null;
        plan.nodes.forEach(function(node) {
          if (node.type === "MaterializeNode") {
            assertFalse(materializeNodeFound);
            assertEqual(nodeDependency.limit, isCluster ? 0 : 3);
            materializeNodeFound = true;
          }
          nodeDependency = node;
        });
        assertTrue(materializeNodeFound);
      },
    };
  };
}());
