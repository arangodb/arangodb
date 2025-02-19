/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue */

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
const db = require("@arangodb").db;
const _ = require("lodash");
const executeJson =  require("@arangodb/aql-helper").executeJson;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  const ruleName = "interchange-adjacent-enumerations";

  // various choices to control the optimizer: 
  const paramNone     = { optimizer: { rules: [ "-all" ] } };
  const paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  const paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  const collectionName = "UnitTestsAhuacatlOptimizer";

  return {

    setUpAll : function () {
      db._drop(collectionName);
      let collection = db._create(collectionName);

      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ value: i });
      }
      collection.insert(docs);
    },

    tearDownAll : function () {
      db._drop(collectionName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      let queries = [ 
        "FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN 1",
        "FOR j IN " + collectionName + " FILTER j.i == 1 FOR i IN " + collectionName + " RETURN j"
      ];
      
      let opts = _.clone(paramNone);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        let result = db._createStatement({query: query, bindVars:  { }, options:  opts}).explain();
        result.plans.forEach(function(plan) {
          assertEqual([ "scatter-in-cluster" ], plan.rules);
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      let queries = [ 
        "FOR i IN 1..10 RETURN i",
        "FOR i IN " + collectionName + " RETURN i",
        "FOR i IN " + collectionName + " FILTER i == 1 FOR j IN " + collectionName + " RETURN i",
        "FOR i IN " + collectionName + " LIMIT 1 FOR j IN " + collectionName + " RETURN i",
        "FOR i IN " + collectionName + " RETURN (FOR j IN " + collectionName + " RETURN j)",
        // the following query must not be optimized because "sub" depends on "i"
        "FOR i IN " + collectionName + " FOR sub IN i FILTER sub.value1 == 'test' && sub.value2 != '' RETURN i",
        "FOR i IN " + collectionName + " FOR sub1 IN i FOR sub2 IN sub1 FILTER sub2.value1 == 'test' && sub2.value2 != '' RETURN i",
        "FOR i IN " + collectionName + " FOR sub1 IN i FOR sub2 IN i FILTER sub2.value1 == 'test' && sub2.value2 != '' RETURN i",
        "FOR i IN " + collectionName + " FOR sub1 IN i FOR sub2 IN i FILTER sub2.value1 == 'test' && sub2.value2 != '' && sub2.value != sub1 RETURN i",
      ];

      let opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        let result = db._createStatement({query: query, bindVars:  { }, options:  opts}).explain();
        result.plans.forEach(function(plan) {
          assertEqual(-1, plan.rules.indexOf(ruleName), query);
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      let queries = [ 
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN i", 1 ],
        [ "FOR i IN 1..10 FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i", 5 ],
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i", 5 ],
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " FOR l IN " + collectionName + " RETURN i", 23 ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN i) RETURN x", 1 ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i) RETURN x", 5 ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " FOR k IN " + collectionName + " RETURN i) FOR y IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN i) RETURN x", 11 ]
      ];
      
      let opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        let withRule = 0;
        let withoutRule = 0;

        let result = db._createStatement({query: query[0], bindVars:  { }, options:  opts}).explain();
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
      let queries = [ 
        [ "FOR i IN " + collectionName + " FOR j IN " + collectionName + " SORT i.value, j.value FILTER i.value == j.value RETURN i.value", [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ],
        [ "FOR j IN " + collectionName + " FOR i IN " + collectionName + " SORT i.value, j.value FILTER i.value == j.value RETURN i.value", [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ],
        [ "FOR x IN (FOR i IN " + collectionName + " FOR j IN " + collectionName + " RETURN { i: i.value, j: j.value }) FILTER x.i == x.j SORT x.i RETURN x.i", [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] ]
      ];
      
      let opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        let planDisabled   = db._createStatement({query: query[0], bindVars:  { }, options:  paramDisabled}).explain();
        let plansEnabled    = db._createStatement({query: query[0], bindVars:  { }, options:  opts}).explain();
        let resultDisabled = db._query(query[0], { }, paramDisabled).toArray();

        assertEqual(-1, planDisabled.plan.rules.indexOf(ruleName), query[0]);
        assertEqual(resultDisabled, query[1]);

        assertTrue(plansEnabled.plans.length > 1);

        // iterate over all plans
        let withRule = 0;
        plansEnabled.plans.forEach(function(plan) {
          let resultEnabled =  executeJson(plan).json;
          assertTrue(isEqual(resultDisabled, resultEnabled), query[0]);
          if (plan.rules.indexOf(ruleName) !== -1) {
            withRule++;
          }
          assertEqual(resultEnabled, query[1]);
        });
          
        assertTrue(withRule > 0);

      });
    }

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
