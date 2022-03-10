/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for late document materialization arangosearch rule
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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


(function() {
  var db = require("@arangodb").db;
  const analyzers = require("@arangodb/analyzers");
  const internal = require('internal');

  return function (isCluster, newDbName, dbArgs) {
    const ruleName = "late-document-materialization-arangosearch";
    const cn = "UnitTestsCollection";
    const cn1 = "UnitTestsCollection1";
    const cn2 = "UnitTestsCollection2";
    const vn = "UnitTestsView";
    const svn = "SortedTestsView";
    const lvn = "LimitTestView";
    return {
      setUpAll : function () {
        db._useDatabase("_system");
        try { db._dropDatabase(newDbName); } catch (err) {}
        db._createDatabase(newDbName, dbArgs);
        db._useDatabase(newDbName);
        
        let c = db._create(cn, { numberOfShards: 3 });
        let c2 = db._create(cn1, { numberOfShards: 3 });
        let c3 = db._create(cn2, { numberOfShards: 3 });
        
        db._createView(vn, "arangosearch", { 
          links: { 
            [cn] : { includeAllFields: true, analyzers : [ "identity" ],
                     fields : { str : { "analyzers" : [ "text_en" ] }}, }, 
            [cn1] : { includeAllFields: true, analyzers : [ "identity" ],
                      fields : { str : { "analyzers" : [ "text_en" ] }}} 
          }});
        db._createView(svn, "arangosearch", {
          consolidationIntervalMsec: 5000, 
          primarySort: [{"field": "str", "direction": "asc"}], 
          links: { 
            [cn] : { includeAllFields: true }, 
            [cn1] : { includeAllFields: true } 
          }});
          
        db._createView(lvn, "arangosearch", {
          consolidationIntervalMsec: 5000, 
          links: { 
            [cn2] : { includeAllFields: true } 
          }});

        c.save({ _key: 'c0',  str: 'cat cat cat cat cat cat cat cat dog', value: 0 });
        c2.save({_key: 'c_0', str: 'cat cat cat cat cat cat cat rat dog', value: 10 });
        c.save({ _key: 'c1',  str: 'cat cat cat cat cat cat pig rat dog', value: 1 });
        c2.save({_key: 'c_1', str: 'cat cat cat cat cat pot pig rat dog', value: 11 });
        c.save({ _key: 'c2',  str: 'cat cat cat cat dot pot pig rat dog', value: 2 });
        c2.save({_key: 'c_2', str: 'cat cat cat fat dot pot pig rat dog', value: 12 });
        c.save({ _key: 'c3',  str: 'cat cat map fat dot pot pig rat dog', value: 3 });
        c2.save({_key: 'c_3', str: 'cat ant map fat dot pot pig rat dog', value: 13 });
        
        let c3data = [];
        for (let i = 0 ; i < 1000; i++) {
          c3data.push({value: i, text: "value" + i});
        }
        c3.insert(c3data);
        
        // trigger view sync
        db._query("FOR d IN " + vn + " OPTIONS { waitForSync: true } RETURN d");
        db._query("FOR d IN " + svn + " OPTIONS { waitForSync: true } RETURN d");
        db._query("FOR d IN " + lvn + " OPTIONS { waitForSync: true } RETURN d");
      },

      tearDownAll : function () {
        db._useDatabase("_system");
        db._dropDatabase(newDbName);
      }, 
      testIssue10845() {
        // this tests a regression described in https://github.com/arangodb/arangodb/issues/10845#issuecomment-575723029:
        // when there is a view in a query, all LIMITs in the query may be inspected.
        // however, each LIMIT was supposed to be present underneath a FOR loop, otherwise
        // the query could crash. This test is here to just make sure there are no crashes.
        let query = "LET results = (FOR d IN " + vn  + " SORT BM25(d) DESC RETURN d) FOR p IN results " +
          "LET validFrom = (RETURN IS_NULL(p.valid_from) ? 0 : p.valid_from) " +
          "LET validTo = (RETURN IS_NULL(p.valid_to) ? 253370764800000 : p.valid_to) " + 
          "LET inactive = (RETURN (validFrom[0] > 1579279781654 || validTo[0] < 1579279781654)) " +
          "RETURN (inactive[0]) ? MERGE(p, {inactive: true}) : p";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToFilter() {
        let query = "FOR d IN " + vn  + " FILTER d.value IN [1,2] SORT BM25(d) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToSort() {
        let query = "FOR d IN " + vn  + " SEARCH d.value IN [1,2] SORT d.value LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToCalculation() {
        let query = "FOR d IN " + vn  + " LET c = d.value + RAND()  SORT BM25(d) LIMIT 10 RETURN c";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNoSort() {
        let query = "FOR d IN " + vn  + " LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToUsedInInnerSort() {
        let query = "FOR d IN " + vn  + " SORT d.value ASC SORT BM25(d) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNoLimit() {
        let query = "FOR d IN " + vn  + " SORT BM25(d)  RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToLimitOnWrongNode() {
        let query = "FOR d IN " + vn  + " SEARCH d.value IN [1, 2, 11, 12] " + 
                    " SORT BM25(d) LET c = BM25(d) * 2 SORT d.str + c LIMIT 10 RETURN { doc: d, sc: c}";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedInSortedView() {
        let query = "FOR d IN " + svn  + " SORT d.str ASC LIMIT 1,10  RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testQueryResultsWithRandomSort() {
        let query = "FOR d IN " + vn  + " SEARCH d.value IN [1,2, 11,12] SORT RAND() LIMIT 10 RETURN d";
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
        let query = "FOR d IN " + vn  + " SEARCH d.value IN [1,2, 11, 12] SORT BM25(d) LIMIT 10 RETURN d";
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
        let query = "FOR d IN " + vn  + " SEARCH d.value IN [1,2, 11,12] SORT BM25(d) LIMIT 10 SORT NOOPT(d.value) ASC RETURN d";
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
        let query = "FOR d IN " + vn  + " SEARCH PHRASE(d.str, 'cat', 'text_en') " +
                    "SORT BM25(d) LIMIT 10 SORT TFIDF(d) DESC LIMIT 4 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        let nodeDependency  = null;
        plan.nodes.forEach(function(node) {
          if (node.type === "MaterializeNode") {
            // there should be no materializer before (e.g. double materialization)
            assertFalse(materializeNodeFound);
            materializeNodeFound = true;
            // the other sort node should be limited but not have a materializer
            // BM25 node on single and TFIDF on cluster as for cluster
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
        let expectedKeys = ['c0', 'c_0', 'c1', 'c_1'];
        result.json.forEach(function(doc) {
          assertEqual(expectedKeys[0], doc._key);
          expectedKeys.shift();
        });
        assertEqual(0, expectedKeys.length);
      },
      testQueryResultsWithMultipleCollectionsAfterCalc() {
        let query = "FOR d IN " + vn  + " SEARCH d.value IN [1,2, 11, 12] SORT BM25(d) LIMIT 10 LET c = CONCAT(NOOPT(d._key), '-C') RETURN c";
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
      testQueryResultsWithBetweenCalc() {
        let query = "FOR d IN " + svn  + " SEARCH d.value IN [1,2, 11, 12] SORT BM25(d) LET c = NOOPT(CONCAT(d._key, '-C')) LIMIT 10  RETURN c";
        let plan = AQL_EXPLAIN(query).plan;
        if (!isCluster) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query);
          assertEqual(4, result.json.length);
          let expected = new Set(['c1-C', 'c2-C', 'c_1-C', 'c_2-C']);
          result.json.forEach(function(doc) {
            assertTrue(expected.has(doc));
            expected.delete(doc);
          });
          assertEqual(0, expected.size);
        } else {
          // on cluster this will not be applied as calculation node will be moved up
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      },
      testQueryResultsSkipSome() {
        let query = "FOR d IN " + vn  + " SEARCH PHRASE(d.str, 'cat', 'text_en')  SORT TFIDF(d) DESC LIMIT 4, 1 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(1, result.json.length);
        assertEqual(result.json[0]._key, 'c2');
      },
      testQueryResultsSkipAll() {
        let query = "FOR d IN " + vn  + " SEARCH d.value IN [1,2, 11, 12] SORT BM25(d) LIMIT 5,10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(0, result.json.length);
      },
      testQueryResultsInInnerLoop() {
        let query = "FOR c IN " + svn + " SEARCH c.value == 1 " +
                      " FOR d IN " + vn  + " SEARCH d.value IN [c.value, c.value + 1] SORT BM25(d) LIMIT 10 RETURN d";
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
      testQueryResultsInOuterLoop() {
        let query = "FOR c IN " + svn + " SEARCH c.value == 1 SORT BM25(c) LIMIT 10 " +
                      " FOR d IN " + vn  + " SEARCH d.value IN [c.value, c.value + 1] RETURN d";
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
      testQueryResultsInSubquery() {
        let query = "FOR c IN " + svn + " SEARCH c.value == 1 " +
                      "LET values = (FOR d IN " + vn  + " SEARCH d.value IN [c.value, c.value + 1] SORT BM25(d) LIMIT 10 RETURN d._key) " +
                    "RETURN values";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json[0].length);
        let expected = new Set(['c1', 'c2']);
        result.json[0].forEach(function(doc) {
          assertTrue(expected.has(doc));
          expected.delete(doc);
        });
        assertEqual(0, expected.size);
      },
      testQueryResultsMultipleLimits() {
        let query = " FOR d IN " + vn  + " SEARCH d.value > 5 SORT BM25(d) " +
                    " LIMIT 1, 5 SORT TFIDF(d) LIMIT 1, 3 SORT NOOPT(d.value) DESC  " +
                    " LIMIT 1, 1 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        let nodeDependency = null;
        // sort by TFIDF node`s limit must be appended with materializer (identified by limit value = 3)
        // as last SORT needs materialized document
        // and SORT by BM25 is not lowest possible variant
        // However in cluster only first sort suitable, as later sorts depend 
        // on all db servers results and performed on coordinator
        plan.nodes.forEach(function(node) {
          if( node.type === "MaterializeNode") {
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
        let query = " FOR d IN " + vn  + " SEARCH d.value > 5 SORT BM25(d) " +
                    " LIMIT 1, 5 SORT TFIDF(d) LIMIT 1, 3 " +
                    " RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        // sort by TFIDF node`s limit must be appended with materializer (identified by limit value = 3)
        // as SORT by BM25 is not lowest possible variant
        // However in cluster only first sort suitable, as later sorts depend 
        // on all db servers results and performed on coordinator
        let nodeDependency = null;
        plan.nodes.forEach(function(node) {
          if( node.type === "MaterializeNode") {
            assertFalse(materializeNodeFound);
            assertEqual(nodeDependency.limit, isCluster ? 0 : 3);
            materializeNodeFound = true;
          }
          nodeDependency = node;
        });
        assertTrue(materializeNodeFound);
      },
      testAppliedInSortedViewWithMultiSort() {
        let query = "FOR d IN " + svn  + " SEARCH d.value > 9 SORT d.str ASC LIMIT 1,10 SORT BM25(d) LIMIT 5  RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        let nodeDependency = null;
        plan.nodes.forEach(function(node) {
          if( node.type === "MaterializeNode") {
            assertFalse(materializeNodeFound);
            assertEqual(nodeDependency.limit, isCluster ? 0 : 5);
            materializeNodeFound = true;
          }
          nodeDependency = node;
        });
        assertTrue(materializeNodeFound);
        let result = AQL_EXECUTE(query);
        assertEqual(3, result.json.length);
        let expected = new Set(['c_0', 'c_1', 'c_2']);
        result.json.forEach(function(doc) {
          assertTrue(expected.has(doc._key));
          expected.delete(doc._key);
        });
        assertEqual(0, expected.size);
      },
      testNoDanglingCalculationNode() {
        let query = "FOR d IN " + svn  + " SEARCH d.value > 9 SORT d.str ASC LIMIT 5  RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        plan.nodes.forEach(function(node) {
          // should be no redundand calculation nodes for single or oneshard server
          if (!isCluster) {
            assertNotEqual(node.type, "CalculationNode");
          }
          if( node.type === "MaterializeNode") {
            materializeNodeFound = true;
          }
        });
        assertFalse(materializeNodeFound);
        let result = AQL_EXECUTE(query);
        assertEqual(4, result.json.length);
        let expected = new Set(['c_0', 'c_1', 'c_2', 'c_3']);
        result.json.forEach(function(doc) {
          assertTrue(expected.has(doc._key));
          expected.delete(doc._key);
        });
        assertEqual(0, expected.size);
      },
      testConstrainedSortOnDbServer() {
        let query = "FOR d IN " + vn  + " SEARCH d.value > 9 SORT BM25(d) LIMIT 10 RETURN {key: d._key, value: d.value}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        let nodeDependency = null;
        plan.nodes.forEach(function(node) {
          if( node.type === "MaterializeNode") {
            assertFalse(materializeNodeFound);
            // FIXME: temporary relaxed assertion
            // We need to fix node placement in oneshard mode
            // first. And than we indeed will get expected node placement
            // assertEqual(nodeDependency.type, isCluster ? "SortNode" : "LimitNode");
            assertTrue(nodeDependency.type === "SortNode" ||  nodeDependency.type === "LimitNode");
            materializeNodeFound = true;
          }
          nodeDependency = node;
        });
        assertTrue(materializeNodeFound);
        let result = AQL_EXECUTE(query);
        assertEqual(4, result.json.length);
        let expected = new Set(['c_0', 'c_1', 'c_2', 'c_3']);
        result.json.forEach(function(doc) {
          assertTrue(expected.has(doc.key));
          expected.delete(doc.key);
        });
        assertEqual(0, expected.size);
      },
      // fullCount was too low
      testRegressionBts611() {
        try {
          let query = "FOR d IN " + lvn  + " SEARCH d.value >= 500 SORT BM25(d) LIMIT @limit RETURN d";
          const options = { fullCount: true };
          let result;

          result = AQL_EXECUTE(query, {limit: 1000}, options);
          assertEqual(500, result.json.length);
          assertEqual(500, result.stats.fullCount);

          result = AQL_EXECUTE(query, {limit: 1}, options);
          assertEqual(1, result.json.length);
          assertEqual(500, result.stats.fullCount);
        
          let plan = AQL_EXPLAIN(query, {limit: 1}, options).plan;
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let materializeNodeFound = false;
          let nodeDependency = null;
          plan.nodes.forEach(function(node) {
            if( node.type === "MaterializeNode") {
              assertFalse(materializeNodeFound);
              assertTrue(nodeDependency.type === "LimitNode");
              materializeNodeFound = true;
           }
           nodeDependency = node;
          });
          assertTrue(materializeNodeFound);
        
          if (!internal.debugCanUseFailAt()) {
            return;
          }
          internal.debugClearFailAt();
          internal.debugSetFailAt('MaterializeExecutor::all_fail');
          result = AQL_EXECUTE(query, {limit: 100}, options);
          assertEqual(0, result.json.length);
          assertEqual(500, result.stats.fullCount);
        
          internal.debugClearFailAt();
          internal.debugSetFailAt('MaterializeExecutor::only_one');
          result = AQL_EXECUTE(query, {limit: 100}, options);
          if (isCluster) {
            // for cluster it depends on db servers number as each materializer will issue only one document
            // to not bother with accurate number  - let's check it is not all 100
            // we anyway want to check it just not breaks in case of materialization failure
            assertTrue(result.json.length < 100);
          } else {
            assertEqual(1, result.json.length);
          }
          assertEqual(500, result.stats.fullCount);
        } finally {
          internal.debugClearFailAt();
        }
      }
    };
  };
}());

