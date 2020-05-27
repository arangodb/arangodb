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
let db = require("internal").db;

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
      c.ensureIndex({ type: "skiplist", fields: ["value1"] });
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
      c.ensureIndex({ type: "skiplist", fields: ["value1", "value2"] });
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
