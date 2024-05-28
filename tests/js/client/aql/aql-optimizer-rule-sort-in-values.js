/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue */

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

const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const isEqual = helper.isEqual;
const getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;
const db = require('internal').db;

function optimizerRuleTestSuite () {
  const ruleName = "sort-in-values";

  // various choices to control the optimizer: 
  const paramNone     = { optimizer: { rules: [ "-all" ] } };
  const paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  const paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      const queries = [
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a NOT IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER LENGTH(i.a) >= 3 FILTER i.a IN values RETURN i",
        "LET values = NOOPT(RANGE(1, 100)) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]) FOR i IN 1..100 FILTER i IN values RETURN i" 
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query, options: paramNone}).explain();
        assertEqual([ ], result.plan.rules);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      const queryList = [ 
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a == 'foobar' && i.a IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a == 'foobar' || i.a IN values RETURN i",
        "FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a NOT IN SPLIT('foo,bar,foobar,qux', ',') RETURN i",
        "LET values = RANGE(1, 100) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "FOR i IN 1..100 FILTER i IN RANGE(1, 100) RETURN i",
        "FOR i IN 1..100 FILTER i IN NOOPT(RANGE(1, 100)) RETURN i", 
        "LET values = NOOPT([ 1 ]) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT([ 1, 2 ]) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT([ 1, 2, 3 ]) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT({ }) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT('foobar') FOR i IN 1..100 FILTER i IN values RETURN i", 
        "FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a IN SPLIT('foo,bar,foobar,qux', ',') RETURN i"
      ];

      queryList.forEach(function(query) {
        let result = db._createStatement({query, options: paramEnabled}).explain();
        assertEqual([ ], result.plan.rules, query);
        let allresults = getQueryMultiplePlansAndExecutions(query, {});
        for (let j = 1; j < allresults.results.length; j++) {
          assertTrue(isEqual(allresults.results[0],
                             allresults.results[j]),
                     "whether the execution of '" + query +
                     "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                     " Should be: '" + JSON.stringify(allresults.results[0]) +
                     "' but is: " + JSON.stringify(allresults.results[j]) + "'"
                    );
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      const queries = [ 
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a NOT IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER LENGTH(i.a) >= 3 FILTER i.a IN values RETURN i",
        "LET values = NOOPT(RANGE(1, 100)) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a NOT IN values RETURN i",
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query, options: paramEnabled}).explain();
        assertEqual([ ruleName ], result.plan.rules, query);
        let allresults = getQueryMultiplePlansAndExecutions(query, {});
        for (let j = 1; j < allresults.results.length; j++) {
          assertTrue(isEqual(allresults.results[0],
            allresults.results[j]),
            "whether the execution of '" + query +
            "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
            " Should be: '" + JSON.stringify(allresults.results[0]) +
            "' but is: " + JSON.stringify(allresults.results[j]) + "'"
          );
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test generated plans
////////////////////////////////////////////////////////////////////////////////

    testPlans : function () {
      const query = "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a IN values RETURN i";
      let actual = db._createStatement({query, options: paramEnabled}).explain();
      let nodes = helper.getLinearizedPlan(actual).reverse();

      assertEqual("ReturnNode", nodes[0].type);
      assertEqual("FilterNode", nodes[1].type);

      assertEqual(nodes[2].outVariable.id, nodes[1].inVariable.id);

      // the filter calculation
      assertEqual("CalculationNode", nodes[2].type);
      assertEqual("compare in", nodes[2].expression.type);
      assertTrue(nodes[2].expression.sorted);
      assertEqual("reference", nodes[2].expression.subNodes[1].type);
      assertEqual("simple", nodes[2].expressionType);
      let varId = nodes[2].expression.subNodes[1].id;

      assertEqual("EnumerateListNode", nodes[3].type);
      assertEqual("CalculationNode", nodes[4].type);

      // new calculation
      assertEqual("CalculationNode", nodes[5].type);
      assertEqual("function call", nodes[5].expression.type);
      assertEqual("SORTED_UNIQUE", nodes[5].expression.name);
      assertEqual(nodes[5].outVariable.id, varId);

      // original calculation
      assertEqual("CalculationNode", nodes[6].type);
      assertEqual("function call", nodes[6].expression.type);
      assertEqual("NOOPT", nodes[6].expression.name);
      assertNotEqual(nodes[6].outVariable.id, varId);

      assertEqual("SingletonNode", nodes[7].type);
      
      let allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (let j = 1; j < allresults.results.length; j++) {
        assertTrue(isEqual(allresults.results[0],
                           allresults.results[j]),
                   "whether the execution of '" + query +
                   "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
                   " Should be: '" + JSON.stringify(allresults.results[0]) +
                   "' but is: " + JSON.stringify(allresults.results[j]) + "'"
                  );
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      let groups = [ ], numbers = [ ], strings = [ ], reversed = [ ], i;
      for (i = 1; i <= 100; ++i) {
        numbers.push(i);
        strings.push("test" + i);
        reversed.push("test" + (101 - i));
      }
      for (i = 0; i < 10; ++i) {
        groups.push(i);
      }

      let queries = [
        [ "LET values = NOOPT(RANGE(1, 100)) FOR i IN 1..100 FILTER i IN values RETURN i", numbers ], 
        [ "LET values = NOOPT(" + JSON.stringify(numbers) + ") FOR i IN 1..100 FILTER i IN values RETURN i", numbers ], 
        [ "LET values = NOOPT(" + JSON.stringify(numbers) + ") FOR i IN 1..100 FILTER i NOT IN values RETURN i", [ ] ], 
        [ "LET values = NOOPT(" + JSON.stringify(strings) + ") FOR i IN 1..100 FILTER i IN values RETURN i", [ ] ], 
        [ "LET values = NOOPT(" + JSON.stringify(strings) + ") FOR i IN 1..100 FILTER i NOT IN values RETURN i", numbers ], 
        [ "LET values = NOOPT(" + JSON.stringify(strings) + ") FOR i IN 1..100 FILTER CONCAT('test', i) IN values RETURN i", numbers ], 
        [ "LET values = NOOPT(" + JSON.stringify(strings) + ") FOR i IN 1..100 FILTER CONCAT('test', i) NOT IN values RETURN i", [ ] ], 
        [ "LET values = NOOPT(" + JSON.stringify(numbers) + ") FOR i IN 1..100 FILTER CONCAT('test', i) IN values RETURN i", [ ] ], 
        [ "LET values = NOOPT(" + JSON.stringify(numbers) + ") FOR i IN 1..100 FILTER CONCAT('test', i) NOT IN values RETURN i", numbers ], 
        [ "LET values = NOOPT(" + JSON.stringify(reversed) + ") FOR i IN 1..100 FILTER CONCAT('test', i) IN values RETURN i", numbers ], 
        [ "LET values = NOOPT(" + JSON.stringify(reversed) + ") FOR i IN 1..100 FILTER CONCAT('test', i) NOT IN values RETURN i", [ ] ], 
        [ "LET values = NOOPT(" + JSON.stringify(reversed) + ") FOR i IN 1..100 FILTER i IN values RETURN i", [ ] ], 
        [ "LET values = NOOPT(" + JSON.stringify(reversed) + ") FOR i IN 1..100 FILTER i NOT IN values RETURN i", numbers ],
        [ "LET values = NOOPT(" + JSON.stringify(numbers) + ") FOR i IN 1..100 FILTER i IN values COLLECT group = i % 10 RETURN group", groups ] 
      ];

      queries.forEach(function(query) {
        let planDisabled   = db._createStatement({query: query[0], options: paramDisabled}).explain();
        let planEnabled    = db._createStatement({query: query[0], options: paramEnabled}).explain();
        let resultDisabled = db._query(query[0], {}, paramDisabled).toArray();
        let resultEnabled  = db._query(query[0], {}, paramEnabled).toArray();

        assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);

        assertTrue(planDisabled.plan.rules.indexOf(ruleName) === -1, query[0]);
        assertTrue(planEnabled.plan.rules.indexOf(ruleName) !== -1, query[0]);

        assertEqual(resultDisabled, query[1], query[0]);
        assertEqual(resultEnabled, query[1], query[0]);
        let allresults = getQueryMultiplePlansAndExecutions(query[0], {});
        for (let j = 1; j < allresults.results.length; j++) {
          assertTrue(isEqual(allresults.results[0],
            allresults.results[j]),
            "whether the execution of '" + query[0] +
            "' this plan gave the wrong results: " + JSON.stringify(allresults.plans[j]) +
            " Should be: '" + JSON.stringify(allresults.results[0]) +
            "' but is: " + JSON.stringify(allresults.results[j]) + "'"
          );
        }
      });
    },
    
    testAvoidDuplicateSortWithSorted : function () {
      let query = "LET values = SORTED(1..100) FOR j IN 1..100 FILTER j IN values RETURN j";

      let plan = db._createStatement({query, options: {optimizer: {rules: ["-move-filters-into-enumerate"] } } }).explain().plan;
      // rule should not kick in, as input is already sorted
      assertEqual(-1, plan.rules.indexOf(ruleName), plan.rules);

      let nodes = plan.nodes;
      assertEqual("SingletonNode", nodes[0].type);
      assertEqual("CalculationNode", nodes[1].type);
      assertEqual("function call", nodes[1].expression.type);
      assertEqual("SORTED", nodes[1].expression.name);
      assertEqual(1, nodes[1].expression.subNodes.length);
      assertEqual("array", nodes[1].expression.subNodes[0].type);
      assertEqual(1, nodes[1].expression.subNodes[0].subNodes.length);
      assertEqual("range", nodes[1].expression.subNodes[0].subNodes[0].type);
      
      let outVariableOfSort = nodes[1].outVariable.id;

      assertEqual("CalculationNode", nodes[4].type);
      assertEqual("compare in", nodes[4].expression.type);
      assertTrue(nodes[4].expression.sorted);
      assertEqual("reference", nodes[4].expression.subNodes[0].type);
      assertEqual("j", nodes[4].expression.subNodes[0].name);
      assertEqual("reference", nodes[4].expression.subNodes[1].type);
      assertEqual(outVariableOfSort, nodes[4].expression.subNodes[1].id);
    },
    
    testAvoidDuplicateSortWithSortedUnique : function () {
      let query = "LET values = SORTED_UNIQUE(1..100) FOR j IN 1..100 FILTER j IN values RETURN j";

      let plan = db._createStatement({query, options: {optimizer: {rules: ["-move-filters-into-enumerate"] } } }).explain().plan;
      // rule should not kick in, as input is already sorted
      assertEqual(-1, plan.rules.indexOf(ruleName), plan.rules);

      let nodes = plan.nodes;
      assertEqual("SingletonNode", nodes[0].type);
      assertEqual("CalculationNode", nodes[1].type);
      assertEqual("function call", nodes[1].expression.type);
      assertEqual("SORTED_UNIQUE", nodes[1].expression.name);
      assertEqual("array", nodes[1].expression.subNodes[0].type);
      assertEqual(1, nodes[1].expression.subNodes[0].subNodes.length);
      assertEqual("range", nodes[1].expression.subNodes[0].subNodes[0].type);
      
      let outVariableOfSort = nodes[1].outVariable.id;

      assertEqual("CalculationNode", nodes[4].type);
      assertEqual("compare in", nodes[4].expression.type);
      assertTrue(nodes[4].expression.sorted);
      assertEqual("reference", nodes[4].expression.subNodes[0].type);
      assertEqual("j", nodes[4].expression.subNodes[0].name);
      assertEqual("reference", nodes[4].expression.subNodes[1].type);
      assertEqual(outVariableOfSort, nodes[4].expression.subNodes[1].id);
    },

    testMovingOutOfLoops : function () {
      let query = "LET values = (FOR i IN 1..10000 RETURN i) FOR j IN 1..100 FILTER j IN values RETURN j";

      let plan = db._createStatement({query, options: {optimizer: {rules: ["-move-filters-into-enumerate"] } } }).explain().plan;
      // rule should kick in, but the SORTED_UNIQUE calculation is expensive
      // and should not be moved into the loop
      assertNotEqual(-1, plan.rules.indexOf(ruleName), plan.rules);

      let nodes = plan.nodes;
      assertEqual("SingletonNode", nodes[0].type);

      // ignore the entire subquery contents
      assertEqual("SubqueryStartNode", nodes[1].type);
      assertEqual("SubqueryEndNode", nodes[4].type);

      assertEqual("CalculationNode", nodes[5].type);
      assertEqual("function call", nodes[5].expression.type);
      assertEqual("SORTED_UNIQUE", nodes[5].expression.name);
      let outVariableOfSort = nodes[5].outVariable.id;

      assertEqual("CalculationNode", nodes[8].type);
      assertEqual("compare in", nodes[8].expression.type);
      assertTrue(nodes[8].expression.sorted);
      assertEqual("reference", nodes[8].expression.subNodes[0].type);
      assertEqual("j", nodes[8].expression.subNodes[0].name);
      assertEqual("reference", nodes[8].expression.subNodes[1].type);
      assertEqual(outVariableOfSort, nodes[8].expression.subNodes[1].id);
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
