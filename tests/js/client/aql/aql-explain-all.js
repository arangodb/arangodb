/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse */
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
let errors = require("internal").errors;
let db = require("@arangodb").db;

/// @brief test suite
function explainSuite () {
  const cn = "UnitTestsAhuacatlExplain";

  function query_explain(query, bindVars = null, options = {}) {
    let stmt = db._createStatement({query, bindVars: bindVars, options: options});
    return stmt.explain();
  };

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
      let actual = query_explain(query).stats;
      assertEqual(1, actual.plansCreated);
      assertTrue(actual.hasOwnProperty("plansCreated"));
      assertTrue(actual.hasOwnProperty("rulesExecuted"));
      assertTrue(actual.hasOwnProperty("rulesSkipped"));
      assertFalse(actual.hasOwnProperty("rules"));
    },
    
    testExplainStatsRules : function () {
      let query = "FOR doc IN " + cn + " FILTER doc.value > 33 RETURN doc";
      let actual = query_explain(query, null, { profile: 2 }).stats;
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
        query_explain(query);
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
      
      let actual = query_explain(query, { foo: "bar" });
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
      let actual = query_explain(query);
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
      actual = query_explain(query, { }, { verbosePlans: true });
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
/// @brief test explain w/ a single plan vs. all plans
////////////////////////////////////////////////////////////////////////////////

    testExplainAllPlansVsSingle : function () {
      const query = "FOR i IN " + cn + " FOR j IN " + cn + " RETURN i";
      
      // single plan
      let actual = query_explain(query, { }, { verbosePlans: true });
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
      actual = query_explain(query, { }, { allPlans: true, verbosePlans: true });
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
  
////////////////////////////////////////////////////////////////////////////////
/// @brief test callstack split
////////////////////////////////////////////////////////////////////////////////

    testExplainCallstackSplit : function () {
      const query = "FOR i IN " + cn + " FOR j IN " + cn + " FILTER i.x == j.x RETURN [i,j]";

      let nodes = query_explain(query, {}, { maxNodesPerCallstack: 1, verbosePlans: true }).plan.nodes;
      assertTrue(Array.isArray(nodes));

      nodes.forEach(function(node) {
        assertTrue(node.isCallstackSplitEnabled || !node.hasOwnProperty("isCallstackSplitEnabled"));
        assertTrue(node.isCallstackSplitEnabled ^ (node.type === "RemoteNode"));
      });
      
      nodes = query_explain(query, {}, { maxNodesPerCallstack: 2, verbosePlans: true }).plan.nodes;
      let shouldHaveCallstackSplitEnabled = false;
      for (let i = nodes.length; i < 0; --i) {
        const node = nodes[i - 1];
        assertTrue(node.hasOwnProperty("isCallstackSplitEnabled"));
        if (node.type === "RemoteNode") {
          shouldHaveCallstackSplitEnabled = false;
        }
        assertEqual(shouldHaveCallstackSplitEnabled, node.isCallstackSplitEnabled);
        shouldHaveCallstackSplitEnabled = !shouldHaveCallstackSplitEnabled;
      }
    },

  };
}

jsunity.run(explainSuite);

return jsunity.done();
