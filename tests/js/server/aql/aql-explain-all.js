/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse, AQL_EXPLAIN */
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
let errors = require("internal").errors;
let db = require("@arangodb").db;

/// @brief test suite
function explainSuite () {
  const cn = "UnitTestsAhuacatlExplain";

  return {

    setUpAll : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDownAll : function () {
      db._drop(cn);
    },

    testExplainStats : function () {
      let query = "FOR doc IN " + cn + " FILTER doc.value > 33 RETURN doc";
      let actual = AQL_EXPLAIN(query).stats;
      assertEqual(1, actual.plansCreated);
      assertTrue(actual.hasOwnProperty("plansCreated"));
      assertTrue(actual.hasOwnProperty("rulesExecuted"));
      assertTrue(actual.hasOwnProperty("rulesSkipped"));
      assertFalse(actual.hasOwnProperty("rules"));
    },
    
    testExplainStatsRules : function () {
      let query = "FOR doc IN " + cn + " FILTER doc.value > 33 RETURN doc";
      let actual = AQL_EXPLAIN(query, null, { profile: 2 }).stats;
      assertEqual(1, actual.plansCreated);
      assertTrue(actual.hasOwnProperty("plansCreated"));
      assertTrue(actual.hasOwnProperty("rulesExecuted"));
      assertTrue(actual.hasOwnProperty("rulesSkipped"));
      assertTrue(actual.hasOwnProperty("rules"));
      let runRules = actual.rules;
      assertTrue(runRules.hasOwnProperty("move-filters-into-enumerate"));
      assertTrue(runRules.hasOwnProperty("remove-filter-covered-by-index"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind parameters
////////////////////////////////////////////////////////////////////////////////

    testExplainBindMissing : function () {
      const query = "RETURN @foo";
      
      try {
        AQL_EXPLAIN(query);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind parameters
////////////////////////////////////////////////////////////////////////////////

    testExplainBindPresent : function () {
      const query = "RETURN @foo";
      
      let actual = AQL_EXPLAIN(query, { foo: "bar" });
      assertEqual(3, actual.plan.nodes.length);
      assertEqual("SingletonNode", actual.plan.nodes[0].type);
      assertEqual("CalculationNode", actual.plan.nodes[1].type);
      assertEqual("ReturnNode", actual.plan.nodes[2].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test verbosity w/ single plan
////////////////////////////////////////////////////////////////////////////////

    testExplainVerbosity : function () {
      const query = "FOR i IN " + cn + " FOR j IN " + cn + " RETURN i";
      
      // single plan, no options
      let actual = AQL_EXPLAIN(query);
      assertTrue(actual.hasOwnProperty("plan"));
      assertFalse(Array.isArray(actual.plan));
      assertTrue(actual.plan.hasOwnProperty("nodes"));
      assertTrue(Array.isArray(actual.plan.nodes));
      assertTrue(actual.plan.hasOwnProperty("rules"));
      assertTrue(Array.isArray(actual.plan.rules));
      assertTrue(actual.plan.hasOwnProperty("estimatedCost"));
      
      actual.plan.nodes.forEach(function(node) {
        assertTrue(node.hasOwnProperty("type"));
        assertFalse(node.hasOwnProperty("typeID")); // deactivated if not verbose
        assertTrue(node.hasOwnProperty("dependencies"));
        assertTrue(Array.isArray(node.dependencies));
        assertFalse(node.hasOwnProperty("parents")); // deactivated if not verbose
        assertTrue(node.hasOwnProperty("id"));
        assertTrue(node.hasOwnProperty("estimatedCost"));
      });

      // single plan, verbose options
      actual = AQL_EXPLAIN(query, { }, { verbosePlans: true });
      assertTrue(actual.hasOwnProperty("plan"));
      assertFalse(Array.isArray(actual.plan));
      assertTrue(actual.plan.hasOwnProperty("nodes"));
      assertTrue(Array.isArray(actual.plan.nodes));
      assertTrue(actual.plan.hasOwnProperty("rules"));
      assertTrue(Array.isArray(actual.plan.rules));

      actual.plan.nodes.forEach(function(node) {
        assertTrue(node.hasOwnProperty("type"));
        assertTrue(node.hasOwnProperty("typeID"));
        assertTrue(node.hasOwnProperty("dependencies"));
        assertTrue(Array.isArray(node.dependencies));
        assertTrue(node.hasOwnProperty("parents"));
        assertTrue(Array.isArray(node.parents));
        assertTrue(node.hasOwnProperty("id"));
        assertTrue(node.hasOwnProperty("estimatedCost"));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain w/ a signle plan vs. all plans
////////////////////////////////////////////////////////////////////////////////

    testExplainAllPlansVsSingle : function () {
      const query = "FOR i IN " + cn + " FOR j IN " + cn + " RETURN i";
      
      // single plan
      let actual = AQL_EXPLAIN(query, { }, { verbosePlans: true });
      assertTrue(actual.hasOwnProperty("plan"));
      assertFalse(actual.hasOwnProperty("plans"));
      assertFalse(Array.isArray(actual.plan));

      assertTrue(actual.plan.hasOwnProperty("nodes"));
      assertTrue(Array.isArray(actual.plan.nodes));

      actual.plan.nodes.forEach(function(node) {
        assertTrue(node.hasOwnProperty("type"));
        assertTrue(node.hasOwnProperty("typeID")); 
        assertTrue(node.hasOwnProperty("dependencies"));
        assertTrue(Array.isArray(node.dependencies));
        assertTrue(node.hasOwnProperty("parents"));
        assertTrue(node.hasOwnProperty("id"));
        assertTrue(node.hasOwnProperty("estimatedCost"));
      });
      
      assertTrue(actual.plan.hasOwnProperty("rules"));
      assertTrue(Array.isArray(actual.plan.rules));

      // multiple plans
      actual = AQL_EXPLAIN(query, { }, { allPlans: true, verbosePlans: true });
      assertFalse(actual.hasOwnProperty("plan"));
      assertTrue(actual.hasOwnProperty("plans"));
      assertTrue(Array.isArray(actual.plans));

      actual.plans.forEach(function (plan) {
        assertTrue(plan.hasOwnProperty("nodes"));
        assertTrue(Array.isArray(plan.nodes));
      
        plan.nodes.forEach(function(node) {
          assertTrue(node.hasOwnProperty("type"));
          assertTrue(node.hasOwnProperty("typeID")); 
          assertTrue(node.hasOwnProperty("dependencies"));
          assertTrue(Array.isArray(node.dependencies));
          assertTrue(node.hasOwnProperty("parents"));
          assertTrue(node.hasOwnProperty("id"));
          assertTrue(node.hasOwnProperty("estimatedCost"));
        });

        assertTrue(plan.hasOwnProperty("rules"));
        assertTrue(Array.isArray(plan.rules));
      });
    },

  };
}

jsunity.run(explainSuite);

return jsunity.done();
