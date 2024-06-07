/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let {db, isCluster} = require("internal");

const ruleName = "move-filters-into-enumerate";
const lateRuleName = "late-document-materialization";
const cn = "UnitTestsCollection";

function optimizerRuleTestSuite () {
  return {

    setUpAll : function () {
      db._drop(cn);
      let c = db._create(cn, { numberOfShards: 2 });
      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: i });
      }
      c.insert(docs);
      c.ensureIndex({ type: "persistent", fields: ["value1"] });
    },
    
    tearDownAll : function () {
      db._drop(cn);
    },
    
    testDoesNotApplyBecauseOfIndexes : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value1 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 0 && doc.value1 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 1000 && doc.value1 < 2000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 1995 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 1995 && doc.value1 < 2000 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 100 && doc.value1 < 200 RETURN doc`, 100 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 < 1000 LIMIT 10 RETURN doc`, 10 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 < 1000 LIMIT 10, 10 RETURN doc`, 10 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 0 && doc.value1 < 1000 LIMIT 100 RETURN doc`, 100 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 0 && doc.value1 < 1000 LIMIT 10, 100 RETURN doc`, 100 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 990 && doc.value1 < 1000 LIMIT 10, 5 RETURN doc`, 0 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 990 && doc.value1 < 1000 LIMIT 5, 5 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 990 && doc.value1 < 1000 LIMIT 5, 10 RETURN doc`, 5 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query[0]).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
        result = db._query(query[0]).toArray().length;
        assertEqual(query[1], result, query);
      });
    },
    
    testDoesNotApplyBecauseOfNonDeterministic : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER RAND() >= -1.0 && doc.value2 == 1999 RETURN doc`, 1 ],
        [ `FOR doc IN ${cn} FILTER RAND() >= -1.0 && doc.value2 >= 1995 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER DOCUMENT('${cn}/test0')._key == NOOPT(doc._key) RETURN doc`, 1 ],
        [ `FOR doc IN ${cn} FILTER DOCUMENT('${cn}/test0')._key == NOOPT(doc._key) && doc.value2 == 0 RETURN doc`, 1 ],
        [ `FOR doc IN ${cn} FILTER DOCUMENT('${cn}/test123')._key != 'test0' && doc.value1 >= 1995 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER DOCUMENT('${cn}/test123')._key != 'test0' && doc.value2 >= 1995 RETURN doc`, 5 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query[0]).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result = db._query(query[0]).toArray().length;
        assertEqual(query[1], result, query);
      });
    },
    
    testLimit : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value2 < 1000 LIMIT 10 RETURN doc`, 10 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 < 1000 LIMIT 10, 10 RETURN doc`, 10 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 0 && doc.value2 < 1000 LIMIT 100 RETURN doc`, 100 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 0 && doc.value2 < 1000 LIMIT 10, 100 RETURN doc`, 100 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 990 && doc.value2 < 1000 LIMIT 10, 5 RETURN doc`, 0 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 990 && doc.value2 < 1000 LIMIT 5, 5 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 990 && doc.value2 < 1000 LIMIT 5, 10 RETURN doc`, 5 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], bindVars: null, options: { optimizer: { rules: ["-" + ruleName] } }}).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result = db._query(query[0], null, { optimizer: { rules: ["-" + ruleName] } }).toArray().length;
        assertEqual(query[1], result, query);
        
        result = db._createStatement(query[0]).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = db._query(query[0]).toArray().length;
        assertEqual(query[1], result, query);
      });
    },
    
    testCollect : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value2 < 1000 COLLECT g = doc.value1 % 100 RETURN g`, 100 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 < 1000 COLLECT WITH COUNT INTO l RETURN l`, 1 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query[0]).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = db._query(query[0]).toArray().length;
        assertEqual(query[1], result, query);
      });
    },
    
    testDisabled : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value2 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 0 && doc.value2 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 0 && doc.value2 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 1000 && doc.value2 < 2000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 1995 && doc.value2 < 2000 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 100 && doc.value2 < 200 RETURN doc`, 100 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 0 && doc.value1 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 1000 && doc.value1 < 2000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 1995 && doc.value1 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 100 && doc.value1 < 200 RETURN doc`, 100 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], bindVars: null, options: { optimizer: { rules: ["-" + ruleName] } }}).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(1, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = db._query(query[0], null, { optimizer: { rules: ["-" + ruleName] } }).toArray().length;
        assertEqual(query[1], result, query);
      });
    },

    testResults : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value2 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 0 && doc.value2 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 0 && doc.value2 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 1000 && doc.value2 < 2000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 1995 && doc.value2 < 2000 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 >= 100 && doc.value2 < 200 RETURN doc`, 100 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 0 && doc.value1 < 1000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 1000 && doc.value1 < 2000 RETURN doc`, 1000 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 1995 && doc.value1 RETURN doc`, 5 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 >= 100 && doc.value1 < 200 RETURN doc`, 100 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query[0]).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = db._query(query[0]).toArray().length;
        assertEqual(query[1], result, query);
      });
    },
    
    testNestedLoopFilters : function () {
      let queries = [ 
        `FOR doc1 IN ${cn} FILTER doc1.abc >= 35 FOR doc2 IN ${cn} RETURN doc1`,
        `FOR doc1 IN ${cn} FILTER doc1.abc >= 35 FILTER doc1.xyz < 19 FOR doc2 IN ${cn} RETURN doc1`,
        `FOR doc1 IN ${cn} FILTER doc1.abc >= 35 FOR doc2 IN ${cn} FILTER doc2.xyz < 12 RETURN doc1`,
        `FOR doc1 IN ${cn} FILTER doc1.abc >= 35 FILTER doc1.xyz < 19 FOR doc2 IN ${cn} FILTER doc2.xyz < 12 FILTER doc2.abc > 4 RETURN doc1`,
        `FOR doc1 IN ${cn} FOR doc2 IN ${cn} FILTER doc2.xyz < 12 RETURN doc1`,
        `FOR doc1 IN ${cn} FOR doc2 IN ${cn} FILTER doc2.xyz < 12 FILTER doc2.abc > 4 RETURN doc1`,
        `FOR doc1 IN ${cn} FOR doc2 IN ${cn} FILTER doc1.abc == doc2.abc RETURN doc1`,
        `FOR i IN 1..10 FOR doc1 IN ${cn} FILTER doc1.abc == i FOR doc2 IN ${cn} FILTER doc2.abc == doc1.abc RETURN doc1`,
        `FOR i IN 1..10 FOR doc1 IN ${cn} FILTER doc1.abc == i FOR doc2 IN ${cn} FILTER doc2.abc == i RETURN doc1`,
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query, bindVars: null, options: { optimizer: { rules: [ "-interchange-adjacent-enumerations" ] } }}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        // all filters should be removed here, and moved into the respective FOR loops
        assertEqual(0, result.plan.nodes.filter((node) => node.type === 'FilterNode').length);
      });
    },
    
    testFiltersWithIndex : function () {
      let queries = [ 
        `FOR doc1 IN ${cn} FILTER doc1.value1 == 35 && doc1.abc >= 35 RETURN doc1`,
        `FOR doc1 IN ${cn} FILTER doc1.value1 >= 35 && doc1.abc >= 35 RETURN doc1`,
        `FOR doc1 IN ${cn} FILTER doc1.value1 IN [1, 2] && doc1.abc >= 35 RETURN doc1`,
        `FOR doc1 IN ${cn} FILTER doc1.value1 >= 35 && doc1.value1 < 44 FILTER doc1.abc >= 35 RETURN doc1`,
        `FOR doc1 IN ${cn} FILTER doc1.value1 == 35 FILTER doc1.abc >= 35 FOR doc2 IN ${cn} FILTER doc2.value1 < doc1.value1 FILTER doc2.abc == 99 RETURN doc1`,
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query, bindVars: null, options: { optimizer: { rules: [ "-interchange-adjacent-enumerations" ] } }}).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
        assertNotEqual(0, result.plan.nodes.filter((node) => node.type === 'IndexNode').length, query);
        // all filters should be removed here, and moved into the respective FOR loops
        assertEqual(0, result.plan.nodes.filter((node) => node.type === 'FilterNode').length, query);
        assertEqual(0, result.plan.nodes.filter((node) => node.type === 'EnumerateCollectionNode').length, query);
      });
    },
    
    testFiltersNoEarlyPruning : function () {
      let queries = [
        // cannot pull FILTER into first FOR loop, because "inner" is not available in FOR loop yet
        `FOR doc1 IN ${cn} FOR inner IN 1..10 FILTER doc1.abc == NOOPT(inner) RETURN doc1`,
        `FOR doc1 IN ${cn} FOR inner IN 1..10 FILTER doc1.xyz == NOOPT(inner) RETURN doc1`,
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query, bindVars: null, options: { optimizer: { rules: [ "-interchange-adjacent-enumerations" ] } }}).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
        assertEqual(1, result.plan.nodes.filter((node) => node.type === 'EnumerateCollectionNode').length, query);
        assertNotEqual(0, result.plan.nodes.filter((node) => node.type === 'FilterNode').length, query);
      });
    },
    
    testFiltersWithIndexNoEarlyPruning : function () {
      let queries = [
        // cannot pull FILTER into first FOR loop, because "inner" is not available in FOR loop yet
        `FOR doc1 IN ${cn} FILTER doc1.value1 == 35 FOR inner IN 1..10 FILTER doc1.abc == NOOPT(inner) RETURN doc1`,
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query, bindVars: null, options: { optimizer: { rules: [ "-interchange-adjacent-enumerations" ] } }}).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
        assertEqual(1, result.plan.nodes.filter((node) => node.type === 'IndexNode').length, query);
        assertNotEqual(0, result.plan.nodes.filter((node) => node.type === 'FilterNode').length, query);
      });
    },

    testVariableUsage : function () {
      let queries = [ 
        `FOR doc IN ${cn} FILTER doc.abc FOR inner IN doc.abc RETURN inner`,
        `FOR doc IN ${cn} FILTER doc.abc FILTER doc.abc FOR inner IN doc.abc RETURN inner`,
        `FOR doc IN ${cn} FILTER doc.abc FILTER doc.xyz FOR inner IN doc.xyz RETURN inner`,
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testVariableUsageInCluster : function () {
      let queries = [ 
        `FOR doc IN ${cn} FOR inner IN 1..10 FILTER doc.abc == inner + 1 RETURN inner`,
        `FOR doc IN ${cn} FOR inner IN 1..10 FILTER doc.abc == inner + 1 FILTER doc.xyz == inner + 2 RETURN inner`,
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query, options: {optimizer: {rules: ["-interchange-adjacent-enumerations"] } } }).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testMultipleFiltersUsageOnIndex : function () {
      for (let i = 1; i < 10; ++i) {
        let filters = "";
        // repeat the same filter i times
        for (let j = 0; j < i; ++j) {
          filters += " FILTER doc.value1 == 'test'";
        }
        let query = `FOR doc IN ${cn} ${filters} RETURN doc`;
        let result = db._createStatement(query).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf("remove-filter-covered-by-index"), query);
        assertNotEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
      }
    },

    testMultipleFiltersUsageOnNonIndex : function () {
      for (let i = 1; i < 10; ++i) {
        let filters = "";
        // repeat the same filter i times
        for (let j = 0; j < i; ++j) {
          filters += " FILTER doc.value2 == 'test'";
        }
        let query = `FOR doc IN ${cn} ${filters} RETURN doc`;
        let result = db._createStatement(query).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      }
    },
    
    testMultipleFiltersUsageMixed : function () {
      for (let i = 2; i < 10; ++i) {
        let filters = "";
        // repeat the same filter i times
        for (let j = 0; j < i; ++j) {
          filters += ` FILTER doc.value${1 + (j % 2)} == 'test'`;
        }
        let query = `FOR doc IN ${cn} ${filters} RETURN doc`;
        let result = db._createStatement(query).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf("remove-filter-covered-by-index"), query);
        assertNotEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
      }
    },

  };
}

function optimizerRuleIndexesTestSuite () {
  return {

    setUpAll : function () {
      db._drop(cn);
      let c = db._create(cn, { numberOfShards: 2 });
      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: i, value3: i });
      }
      c.insert(docs);
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
    },
    
    tearDownAll : function () {
      db._drop(cn);
    },
    
    testIndexesDoesNotApplyBecauseOfIndexes : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value1 == 1000 && doc.value2 == 1000 RETURN doc`, 1 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 == 1000 && doc.value2 == 2000 RETURN doc`, 0 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 == 1000 && doc.value2 >= 1000 RETURN doc`, 1 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query[0]).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
        result = db._query(query[0]).toArray().length;
        assertEqual(query[1], result, query);
      });
    },
    
    testIndexesLimitWithLateMaterialization : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value1 == 1000 SORT doc.value2 LIMIT 0, 10 RETURN doc`, 1 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 IN [ 1000, 1001, 1002 ] SORT doc.value2 LIMIT 0, 10 RETURN doc`, 3 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], bindVars: null, options: { optimizer: { rules: ["-use-index-for-sort"] } }}).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf("push-down-late-materialization"), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = db._query(query[0], null, { optimizer: { rules: ["-use-index-for-sort"] } }).toArray().length;
        assertEqual(query[1], result, query);
      });
    },
    
    testIndexesLimitWithoutLateMaterialization : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value1 == 1000 && doc.value3 >= 1000 SORT doc.value2 LIMIT 0, 10 RETURN doc`, 1 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 == 1000 && doc.value3 >= 1500 SORT doc.value2 LIMIT 0, 10 RETURN doc`, 0 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 IN [ 1000, 1001, 1002 ] && doc.value3 >= 1000 SORT doc.value2 LIMIT 0, 10 RETURN doc`, 3 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 IN [ 1000, 1001, 1002 ] && doc.value3 >= 1500 SORT doc.value2 LIMIT 0, 10 RETURN doc`, 0 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], bindVars: null, options: { optimizer: { rules: ["-" + ruleName] } }}).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        result = db._query(query[0], null, { optimizer: { rules: ["-" + ruleName] } }).toArray().length;
        assertEqual(query[1], result, query);
        
        result = db._createStatement(query[0]).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = db._query(query[0]).toArray().length;
        assertEqual(query[1], result, query);
      });
    },
    
  };
}

function optimizerRuleListTestSuite () {
  return {
    testListDoesNotApplyBecauseOfCondition : function () {
      let queries = [ 
        [ `FOR i IN 1..1000 FILTER i >= RAND() RETURN i`, 1000 ],
        [ `FOR i IN 1..1000 FILTER NOOPT(i) > 0 RETURN i`, 1000 ],
        [ `FOR i IN 1..1000 FILTER NOOPT(i) > 0 LIMIT 500 RETURN i`, 500 ],
        [ `FOR i IN 1..1000 FILTER NOOPT(i) > 0 LIMIT 500, 510 RETURN i`, 500 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query[0]).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(1, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = db._query(query[0]).toArray().length;
        assertEqual(query[1], result, query);
      });
    },
    
    testListResults : function () {
      let queries = [ 
        [ `FOR i IN 1..1000 FILTER i >= 1 && i <= 1000 RETURN i`, 1, 1000 ],
        [ `FOR i IN 1..2000 FILTER i >= 1 && i <= 1000 RETURN i`, 1, 1000 ],
        [ `FOR i IN 1..2002 FILTER i >= 1 && i <= 2000 RETURN i`, 1, 2000 ],
        [ `FOR i IN 1..2002 FILTER i >= 1540 && i <= 1760 RETURN i`, 1540, 1760 ],
        [ `FOR i IN 1..10000 FILTER i >= 1234 && i <= 8999 RETURN i`, 1234, 8999 ],
        [ `FOR i IN 1..2001 FILTER i >= 1500 LIMIT 100 RETURN i`, 1500, 1599 ],
        [ `FOR i IN 1..2001 FILTER i >= 1500 LIMIT 2000 RETURN i`, 1500, 2001 ],
        [ `FOR i IN 1..2000 FILTER i >= 1500 LIMIT 10, 10 RETURN i`, 1510, 1519 ],
        [ `FOR i IN 1..2000 FILTER i >= 0 LIMIT 1500, 10 RETURN i`, 1501, 1510 ],
        [ `FOR i IN 1..2000 FILTER i >= 10 LIMIT 1500, 10 RETURN i`, 1510, 1519 ],
        [ `FOR i IN 1..2000 FILTER i >= 10 && i <= 20 LIMIT 5 RETURN i`, 10, 14 ],
        [ `FOR i IN 1..2000 FILTER i >= 10 && i <= 20 LIMIT 1, 5 RETURN i`, 11, 15 ],
        [ `FOR i IN 1..2000 FILTER i >= 10 && i <= 20 LIMIT 5, 5 RETURN i`, 15, 19 ],
        [ `FOR i IN 1..2000 FILTER i >= 10 && i <= 20 LIMIT 6, 5 RETURN i`, 16, 20 ],
        [ `FOR i IN 1..10000 FILTER i >= 1234 && i <= 8999 LIMIT 1234, 1222 RETURN i`, 2468, 3689 ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query[0]).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        
        result = db._query(query[0]).toArray();
        let lower = query[1];
        let upper = query[2];
        assertEqual(result.length, upper - lower + 1, {length: result.length, query});
        result.forEach((v, i) => {
          assertEqual(v, i + lower, query);
        });
        
        // turn rule off. result should be identical
        result = db._query(query[0], null, {optimizer: {rules: ["-" + ruleName] } }).toArray();
        assertEqual(result.length, upper - lower + 1, {length: result.length, query});
        result.forEach((v, i) => {
          assertEqual(v, i + lower, query);
        });
      });
    },

    testRegressionMDS1230: function () {
      // MDS-1230: subquery was inlined, and then the "FILTER d" was moved into the
      // "FOR d" EnumerateListNode. This made the execution plan look like this
      // (still correct):
      //  Id   NodeType                  Par   Est.   Comment
      //   1   SingletonNode                      1   * ROOT
      //   2   EnumerateCollectionNode     ✓      0     - FOR t IN _analyzers   /* full collection scan (projections: `field`)  */
      //   4   CalculationNode             ✓      0       - LET #9 = t.`field`   /* attribute expression */   /* collections used: t : _analyzers */
      //   5   EnumerateListNode           ✓      0       - FOR #8 IN #9   /* list iteration */   FILTER #9[#8]   /* early pruning */
      //   6   CalculationNode             ✓      0         - LET d = #9[#8]   /* simple expression */
      //  11   ReturnNode                         0         - RETURN d
      // Then projections were added for "t.field", which replace variable #9 with a new one.
      // The bug was that the usage of variable #9 in EnumerateListNode 5's FILTER condition was
      // not correctly replaced.
      const query = `FOR t IN _analyzers FOR d IN (LET o = t.field FOR k IN o RETURN o[k]) FILTER d RETURN d`;

      let result = db._createStatement(query).explain();
      assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
      
      let collections = result.plan.nodes.filter(function(n) { return n.type === 'EnumerateCollectionNode'; });
      assertEqual(1, collections.length);
      let projections = collections[0].projections;
      assertEqual(1, projections.length);
      assertEqual(["field"], projections[0].path);
      let projectionVar = projections[0].variable.id;
      
      let lists = result.plan.nodes.filter(function(n) { return n.type === 'EnumerateListNode'; });
      assertEqual(1, lists.length);
      let filter = lists[0].filter;

      assertEqual("indexed access", filter.type);
      assertEqual(2, filter.subNodes.length);
      assertEqual("reference", filter.subNodes[0].type);
      assertEqual(projectionVar, filter.subNodes[0].id);
    },
    
  };
}

jsunity.run(optimizerRuleTestSuite);
jsunity.run(optimizerRuleIndexesTestSuite);
jsunity.run(optimizerRuleListTestSuite);

return jsunity.done();
