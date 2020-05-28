/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE, AQL_EXECUTEJSON */

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
var db = require("@arangodb").db;
var _ = require("lodash");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "interchange-adjacent-enumerations";

  // various choices to control the optimizer: 
  var paramNone     = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  var collection = null;
  var collectionName = "UnitTestsAhuacatlOptimizer";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      db._drop(collectionName);
      collection = db._create(collectionName);

      for (var i = 0; i < 10; ++i) {
        collection.save({ value: i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(collectionName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [ 
        "FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN 1",
        "FOR j IN " + collectionName + " FILTER j.i == 1 FOR i IN " + collectionName + " RETURN j"
      ];
      
      var opts = _.clone(paramNone);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, opts);
        result.plans.forEach(function(plan) {
          assertEqual([ ], plan.rules);
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [ 
        "FOR i IN 1..10 RETURN i",
        "FOR i IN " + collectionName + " RETURN i",
        "FOR i IN " + collectionName + " FILTER i == 1 FOR j IN " + collectionName + " RETURN i",
        "FOR i IN " + collectionName + " LIMIT 1 FOR j IN " + collectionName + " RETURN i",
        "FOR i IN " + collectionName + " RETURN (FOR j IN " + collectionName + " RETURN j)"
      ];

      var opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, opts);
        result.plans.forEach(function(plan) {
          assertTrue(plan.rules.indexOf(ruleName) === -1, query);
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [ 
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN i", 1 ],
        [ "FOR i IN 1..10 FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i", 5 ],
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i", 5 ],
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " FOR l IN " + collectionName + " RETURN i", 23 ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN i) RETURN x", 1 ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i) RETURN x", 5 ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i) FOR y IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN i) RETURN x", 11 ]
      ];
      
      var opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        var withRule = 0;
        var withoutRule = 0;

        var result = AQL_EXPLAIN(query[0], { }, opts);
        result.plans.forEach(function(plan) {
          if (plan.rules.indexOf(ruleName) === -1) {
            withoutRule++;
          }
          else {
            withRule++;
          }
        });
      
        // there should still be the original plan
        assertEqual(1, withoutRule, query[0]);

        // put there should also be permuted plans
        assertEqual(query[1], withRule, query[0]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResults : function () {
      var queries = [ 
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " SORT i.value, j.value FILTER i.value == j.value RETURN i.value", [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ],
        [ "FOR j IN " + collectionName + " FOR i IN " + collectionName + " SORT i.value, j.value FILTER i.value == j.value RETURN i.value", [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN { i: i.value, j: j.value }) FILTER x.i == x.j SORT x.i RETURN x.i", [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ]
      ];
      
      var opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        var planDisabled   = AQL_EXPLAIN(query[0], { }, paramDisabled);
        var plansEnabled    = AQL_EXPLAIN(query[0], { }, opts);
        var resultDisabled = AQL_EXECUTE(query[0], { }, paramDisabled).json;

        assertTrue(planDisabled.plan.rules.indexOf(ruleName) === -1, query[0]);
        assertEqual(resultDisabled, query[1]);

        assertTrue(plansEnabled.plans.length > 1);

        // iterate over all plans
        var withRule = 0;
        plansEnabled.plans.forEach(function(plan) {
          var resultEnabled = AQL_EXECUTEJSON(plan).json;
          assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);
          if (plan.rules.indexOf(ruleName) !== -1) {
            withRule++;
          }
          assertEqual(resultEnabled, query[1]);
        });
          
        assertTrue(withRule > 0);

      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test many permutations
////////////////////////////////////////////////////////////////////////////////

    testManyPermutations : function () {
      var query = "FOR i IN " + collectionName + " " +
                  "FOR j IN " + collectionName + " " + 
                  "FOR k IN " + collectionName + " " + 
                  "FOR l IN " + collectionName + " RETURN 1";

      var explain = AQL_EXPLAIN(query);
      assertEqual(24, explain.stats.plansCreated); // faculty of 4 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test too many permutations
////////////////////////////////////////////////////////////////////////////////

    testTooManyPermutations : function () {
      var query = "FOR i IN " + collectionName + " " +
                  "FOR j IN " + collectionName + " " + 
                  "FOR k IN " + collectionName + " " + 
                  "FOR l IN " + collectionName + " " + 
                  "FOR m IN " + collectionName + " " + 
                  "FOR n IN " + collectionName + " " + 
                  "FOR o IN " + collectionName + " RETURN 1";

      var explain = AQL_EXPLAIN(query);
      assertEqual(128, explain.stats.plansCreated); // default limit enforced by optimizer
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test too many permutations
////////////////////////////////////////////////////////////////////////////////

    testTooManyPermutationsUnlimited : function () {
      var query = "FOR i IN " + collectionName + " " +
                  "FOR j IN " + collectionName + " " + 
                  "FOR k IN " + collectionName + " " + 
                  "FOR l IN " + collectionName + " " + 
                  "FOR m IN " + collectionName + " " + 
                  "FOR n IN " + collectionName + " " + 
                  "FOR o IN " + collectionName + " RETURN 1";

      var explain = AQL_EXPLAIN(query, null, { maxNumberOfPlans: 9999999 });
      assertEqual(5040, explain.stats.plansCreated); // faculty of 7 
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();

