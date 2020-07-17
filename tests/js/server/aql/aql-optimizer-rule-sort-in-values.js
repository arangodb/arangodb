/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

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

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
var getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "sort-in-values";

  // various choices to control the optimizer: 
  var paramNone     = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a NOT IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER LENGTH(i.a) >= 3 FILTER i.a IN values RETURN i",
        "LET values = NOOPT(RANGE(1, 100)) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]) FOR i IN 1..100 FILTER i IN values RETURN i" 
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual([ ], result.plan.rules);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queryList = [ 
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
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual([ ], result.plan.rules, query);
        var allresults = getQueryMultiplePlansAndExecutions(query, {});
        for (var j = 1; j < allresults.results.length; j++) {
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
      var queries = [ 
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a NOT IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER LENGTH(i.a) >= 3 FILTER i.a IN values RETURN i",
        "LET values = NOOPT(RANGE(1, 100)) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]) FOR i IN 1..100 FILTER i IN values RETURN i", 
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a IN values RETURN i",
        "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a NOT IN values RETURN i",
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual([ ruleName ], result.plan.rules, query);
        var allresults = getQueryMultiplePlansAndExecutions(query, {});
        for (var j = 1; j < allresults.results.length; j++) {
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
      var query = "LET values = NOOPT(SPLIT('foo,bar,foobar,qux', ',')) FOR i IN [ { a: 'foo' }, { a: 'bar' }, { a: 'baz' } ] FILTER i.a IN values RETURN i";
      var actual = AQL_EXPLAIN(query, null, paramEnabled);
      var nodes = helper.getLinearizedPlan(actual).reverse();

      assertEqual("ReturnNode", nodes[0].type);
      assertEqual("FilterNode", nodes[1].type);

      assertEqual(nodes[2].outVariable.id, nodes[1].inVariable.id);

      // the filter calculation
      assertEqual("CalculationNode", nodes[2].type);
      assertEqual("compare in", nodes[2].expression.type);
      assertTrue(nodes[2].expression.sorted);
      assertEqual("reference", nodes[2].expression.subNodes[1].type);
      assertEqual("simple", nodes[2].expressionType);
      var varId = nodes[2].expression.subNodes[1].id;

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
      
      var allresults = getQueryMultiplePlansAndExecutions(query, {});
      for (var j = 1; j < allresults.results.length; j++) {
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
      var groups = [ ], numbers = [ ], strings = [ ], reversed = [ ], i;
      for (i = 1; i <= 100; ++i) {
        numbers.push(i);
        strings.push("test" + i);
        reversed.push("test" + (101 - i));
      }
      for (i = 0; i < 10; ++i) {
        groups.push(i);
      }

      var queries = [
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
        var planDisabled   = AQL_EXPLAIN(query[0], { }, paramDisabled);
        var planEnabled    = AQL_EXPLAIN(query[0], { }, paramEnabled);
        var resultDisabled = AQL_EXECUTE(query[0], { }, paramDisabled).json;
        var resultEnabled  = AQL_EXECUTE(query[0], { }, paramEnabled).json;

        assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);

        assertTrue(planDisabled.plan.rules.indexOf(ruleName) === -1, query[0]);
        assertTrue(planEnabled.plan.rules.indexOf(ruleName) !== -1, query[0]);

        assertEqual(resultDisabled, query[1], query[0]);
        assertEqual(resultEnabled, query[1], query[0]);
        var allresults = getQueryMultiplePlansAndExecutions(query[0], {});
        for (var j = 1; j < allresults.results.length; j++) {
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

      let plan = AQL_EXPLAIN(query).plan;
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

      let plan = AQL_EXPLAIN(query).plan;
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

      let plan = AQL_EXPLAIN(query).plan;
      // rule should kick in, but the SORTED_UNIQUE calculation is expensive
      // and should not be moved into the loop
      assertNotEqual(-1, plan.rules.indexOf(ruleName), plan.rules);

      let nodes = plan.nodes;
      assertEqual("SingletonNode", nodes[0].type);
      // ignore the entire subquery contents
      assertEqual("SubqueryStartNode", nodes[1].type);
      assertEqual("SubqueryEndNode", nodes[4].type);

      assertEqual("CalculationNode", nodes[6].type);
      assertEqual("function call", nodes[6].expression.type);
      assertEqual("SORTED_UNIQUE", nodes[6].expression.name);
      let outVariableOfSort = nodes[6].outVariable.id;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

