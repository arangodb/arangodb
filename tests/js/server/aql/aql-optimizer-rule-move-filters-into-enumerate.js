/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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
      for (let i = 0; i < 2000; ++i) {
        c.insert({ _key: "test" + i, value1: i, value2: i });
      }
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
        let result = AQL_EXPLAIN(query[0]);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
        result = AQL_EXECUTE(query[0]).json.length;
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
        let result = AQL_EXPLAIN(query[0]);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result = AQL_EXECUTE(query[0]).json.length;
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
        let result = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-" + ruleName] } });
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        result = AQL_EXECUTE(query[0], null, { optimizer: { rules: ["-" + ruleName] } }).json.length;
        assertEqual(query[1], result, query);
        
        result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = AQL_EXECUTE(query[0]).json.length;
        assertEqual(query[1], result, query);
      });
    },
    
    testCollect : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value2 < 1000 COLLECT g = doc.value1 % 100 RETURN g`, 100 ],
        [ `FOR doc IN ${cn} FILTER doc.value2 < 1000 COLLECT WITH COUNT INTO l RETURN l`, 1 ],
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = AQL_EXECUTE(query[0]).json.length;
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
        let result = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-" + ruleName] } });
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(1, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = AQL_EXECUTE(query[0], null, { optimizer: { rules: ["-" + ruleName] } }).json.length;
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
        let result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = AQL_EXECUTE(query[0]).json.length;
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
        let result = AQL_EXPLAIN(query, null, { optimizer: { rules: [ "-interchange-adjacent-enumerations" ] } });
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
        let result = AQL_EXPLAIN(query, null, { optimizer: { rules: [ "-interchange-adjacent-enumerations" ] } });
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
        // cannot pull FILTER into FOR loop, because "inner" is not available in FOR loop yet
        `FOR doc1 IN ${cn} FOR inner IN 1..10 FILTER doc1.abc == inner RETURN doc1`,
        `FOR doc1 IN ${cn} FOR inner IN 1..10 FILTER doc1.xyz == inner RETURN doc1`,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, null, { optimizer: { rules: [ "-interchange-adjacent-enumerations" ] } });
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
        assertEqual(1, result.plan.nodes.filter((node) => node.type === 'EnumerateCollectionNode').length, query);
        assertNotEqual(0, result.plan.nodes.filter((node) => node.type === 'FilterNode').length, query);
      });
    },
    
    testFiltersWithIndexNoEarlyPruning : function () {
      let queries = [
        // cannot pull FILTER into FOR loop, because "inner" is not available in FOR loop yet
        `FOR doc1 IN ${cn} FILTER doc1.value1 == 35 FOR inner IN 1..10 FILTER doc1.abc == inner RETURN doc1`,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, null, { optimizer: { rules: [ "-interchange-adjacent-enumerations" ] } });
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
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testVariableUsageInCluster : function () {
      let queries = [ 
        `FOR doc IN ${cn} FOR inner IN 1..10 FILTER doc.abc == inner + 1 RETURN inner`,
        `FOR doc IN ${cn} FOR inner IN 1..10 FILTER doc.abc == inner + 1 FILTER doc.xyz == inner + 2 RETURN inner`,
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        // In these queries something odd happens, due to the distributed
        // nature of the cluster and the cost calculation we use, the execution
        // plan we prefer in the single server becomes more expensive than
        // another plan, which cannot pull the FILTER into the
        // EnumerateCollectionNode. Therefore another plan is chosen and
        // we do not see our rule used. Therefore we have to distinguish
        // cases here:
        if (isCluster()) {
          assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        } else {
          assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        }
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
        let result = AQL_EXPLAIN(query);
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
        let result = AQL_EXPLAIN(query);
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
        let result = AQL_EXPLAIN(query);
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
      for (let i = 0; i < 2000; ++i) {
        c.insert({ _key: "test" + i, value1: i, value2: i, value3: i });
      }
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
        let result = AQL_EXPLAIN(query[0]);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf("use-indexes"), query);
        result = AQL_EXECUTE(query[0]).json.length;
        assertEqual(query[1], result, query);
      });
    },
    
    testIndexesLimitWithLateMaterialization : function () {
      let queries = [ 
        [ `FOR doc IN ${cn} FILTER doc.value1 == 1000 SORT doc.value2 LIMIT 0, 10 RETURN doc`, 1 ],
        [ `FOR doc IN ${cn} FILTER doc.value1 IN [ 1000, 1001, 1002 ] SORT doc.value2 LIMIT 0, 10 RETURN doc`, 3 ],
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-use-index-for-sort"] } });
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = AQL_EXECUTE(query[0], null, { optimizer: { rules: ["-use-index-for-sort"] } }).json.length;
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
        let result = AQL_EXPLAIN(query[0], null, { optimizer: { rules: ["-" + ruleName] } });
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        result = AQL_EXECUTE(query[0], null, { optimizer: { rules: ["-" + ruleName] } }).json.length;
        assertEqual(query[1], result, query);
        
        result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(-1, result.plan.rules.indexOf(lateRuleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = AQL_EXECUTE(query[0]).json.length;
        assertEqual(query[1], result, query);
      });
    },
    
  };
}

jsunity.run(optimizerRuleTestSuite);
jsunity.run(optimizerRuleIndexesTestSuite);

return jsunity.done();
