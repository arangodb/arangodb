/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual */

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

var jsunity = require("jsunity");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerStatsTestSuite () {
  var c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test for-loop
////////////////////////////////////////////////////////////////////////////////

    testForLoop : function () {
      var query = "FOR i IN " + c.name() + " RETURN i";

      var stats = db._createStatement(query).explain().stats;
      assertTrue(stats.hasOwnProperty("plansCreated"));
      assertTrue(stats.hasOwnProperty("rulesExecuted"));
      assertTrue(stats.hasOwnProperty("rulesSkipped"));

      assertEqual(1, stats.plansCreated);
      assertNotEqual(0, stats.rulesExecuted);
      assertEqual(0, stats.rulesSkipped);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test for-loop permutation
////////////////////////////////////////////////////////////////////////////////

    testForLoopPermutation : function () {
      var query = "FOR i IN " + c.name() + " FOR j IN " + c.name() + " RETURN i";

      var stats = db._createStatement(query).explain().stats;
      assertTrue(stats.hasOwnProperty("plansCreated"));
      assertTrue(stats.hasOwnProperty("rulesExecuted"));
      assertTrue(stats.hasOwnProperty("rulesSkipped"));

      assertEqual(2, stats.plansCreated);
      assertNotEqual(0, stats.rulesExecuted);
      assertEqual(0, stats.rulesSkipped);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test for-loop permutation
////////////////////////////////////////////////////////////////////////////////

    testForLoopPermutationDisabled : function () {
      var query = "FOR i IN " + c.name() + " FOR j IN " + c.name() + " RETURN i";

      var stats = db._createStatement({query: query, bindVars:  null, options:  { optimizer: { rules: [ "-interchange-adjacent-enumerations" ] } }}).explain().stats;
      assertTrue(stats.hasOwnProperty("plansCreated"));
      assertTrue(stats.hasOwnProperty("rulesExecuted"));
      assertTrue(stats.hasOwnProperty("rulesSkipped"));

      assertEqual(1, stats.plansCreated);
      assertNotEqual(1, stats.rulesExecuted);
      assertEqual(1, stats.rulesSkipped);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test for-loop permutation
////////////////////////////////////////////////////////////////////////////////

    testForLoopPermutationAllDisabled : function () {
      var query = "FOR i IN " + c.name() + " FOR j IN " + c.name() + " RETURN i";

      var stats = db._createStatement({query: query, bindVars:  null, options:  { optimizer: { rules: [ "-all" ] } }}).explain().stats;
      assertTrue(stats.hasOwnProperty("plansCreated"));
      assertTrue(stats.hasOwnProperty("rulesExecuted"));
      assertTrue(stats.hasOwnProperty("rulesSkipped"));

      assertEqual(1, stats.plansCreated);
      assertTrue(1, stats.rulesExecuted);
      assertNotEqual(1, stats.rulesSkipped);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test for-loop permutation, maxNumberOfPlans
////////////////////////////////////////////////////////////////////////////////

    testForLoopPermutationMaxNumberOfPlans : function () {
      var query = "FOR i IN " + c.name() + " FOR j IN " + c.name() + " RETURN i";

      var stats = db._createStatement({query: query, bindVars:  null, options:  { maxNumberOfPlans: 1 }}).explain().stats;
      assertTrue(stats.hasOwnProperty("plansCreated"));
      assertTrue(stats.hasOwnProperty("rulesExecuted"));
      assertTrue(stats.hasOwnProperty("rulesSkipped"));

      assertEqual(1, stats.plansCreated);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerStatsTestSuite);

return jsunity.done();
