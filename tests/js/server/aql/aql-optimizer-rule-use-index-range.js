/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rule use indexes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2014 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;
var db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleUseIndexRangeTester () {
  var ruleName = "use-indexes";
  var collBaseName = "UTUseIndexRange";
  var collNames = ["NoInd", "SkipInd", "HashInd", "BothInd"];
  var collNoInd;
  var collSkipInd;
  var collHashInd;
  var collBothInd;

  // various choices to control the optimizer: 
  var paramNone     = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramAll      = { optimizer: { rules: [ "+all" ] } };
  var paramEnabledAllPlans = { optimizer: { rules: [ "-all", "+" + ruleName ]},
                               allPlans: true };
  var paramAllAllPlans = { optimizer: { rules: [ "+all" ]}, allPlans: true };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var n = collNames.map(function(x) { return collBaseName + x; });
      var colls = [];
      for (var i = 0; i < n.length; i++) {
        var collName = n[i], coll;
        internal.db._drop(collName);
        coll = internal.db._create(collName);
        for (var j = 0; j < 10; j++) {
          coll.insert({"a":j, "s":"s"+j});
        }
        colls.push(coll);
      }
      collNoInd = colls[0];
      collSkipInd = colls[1];
      collSkipInd.ensureSkiplist("a");
      collHashInd = colls[2];
      collHashInd.ensureHashIndex("a");
      collBothInd = colls[3];
      collBothInd.ensureHashIndex("a");
      collBothInd.ensureSkiplist("a");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      var n = collNames.map(function(x) { return collBaseName + x; });
      for (var i = 0; i < n.length; i++) {
        internal.db._drop(n[i]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [ 
        "FOR i IN UTUseIndexRangeNoInd FILTER i.a >= 2 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a >= 2 RETURN i",
        "FOR i IN UTUseIndexRangeHashInd FILTER i.a == 2 RETURN i",
        "FOR i IN UTUseIndexRangeBothInd FILTER i.a == 2 RETURN i",
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual([ ], removeAlwaysOnClusterRules(result.plan.rules), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [ 
        ["FOR i IN UTUseIndexRangeNoInd FILTER i.a >= 2 RETURN i", true],
        ["FOR i IN UTUseIndexRangeNoInd FILTER i.a == 2 RETURN i", true],
        ["FOR i IN UTUseIndexRangeHashInd FILTER i.a >= 2 RETURN i", false]
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { }, paramEnabled);
        if (db._engine().name !== "rocksdb" || query[1]) {
          assertEqual([ ], removeAlwaysOnClusterRules(result.plan.rules), query);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      var queries = [ 
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a >= 2 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a == 2 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a < 2 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a < 2 && i.a > 0 RETURN i",
        "FOR i IN UTUseIndexRangeHashInd FILTER i.a == 2 RETURN i",
        "FOR i IN UTUseIndexRangeBothInd FILTER i.a == 2 RETURN i",
        "FOR i IN UTUseIndexRangeBothInd FILTER i.a <= 2 RETURN i",
        "FOR i IN UTUseIndexRangeBothInd FILTER i.a > 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual([ ruleName ], removeAlwaysOnClusterRules(result.plan.rules),
                    query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffectWithAll : function () {
      var queries = [ 
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a >= 2 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a == 2 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a < 2 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a < 2 && i.a > 0 RETURN i",
        "FOR i IN UTUseIndexRangeHashInd FILTER i.a == 2 RETURN i",
        "FOR i IN UTUseIndexRangeBothInd FILTER i.a == 2 RETURN i",
        "FOR i IN UTUseIndexRangeBothInd FILTER i.a <= 2 RETURN i",
        "FOR i IN UTUseIndexRangeBothInd FILTER i.a > 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramAll);
        assertTrue(result.plan.rules.indexOf(ruleName) >= 0, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that impossible conditions are detected and optimized
////////////////////////////////////////////////////////////////////////////////

    testImpossibleRangesAreDetected : function () {
      var queries = [ 
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a < 2 && i.a > 3 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a == 7 && i.a != 7 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a == 7 && i.a != 7.0 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a == 7.0 && i.a != 7.0 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a == 7.0 && i.a != 7 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a > 7 && i.a < 7 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a > 7 && i.a < 7.0 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a > 7.0 && i.a < 7.0 RETURN i",
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a > 7.0 && i.a < 7 RETURN i"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramAll);
        var foundNoResults = false;
        for (var t in result.plan.nodes) {
          if (result.plan.nodes[t].type === "NoResultsNode") {
            foundNoResults = true;
          }
        }
        assertTrue(foundNoResults, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that plan explosion does not happen
////////////////////////////////////////////////////////////////////////////////

    testRuleHasNoPlanExplosion : function () {
      var query =
        "LET A = (FOR a IN UTUseIndexRangeBothInd FILTER a.a == 2 RETURN a) "+
        "LET B = (FOR b IN UTUseIndexRangeBothInd FILTER b.a == 2 RETURN b) "+
        "LET C = (FOR c IN UTUseIndexRangeBothInd FILTER c.a == 2 RETURN c) "+
        "LET D = (FOR d IN UTUseIndexRangeBothInd FILTER d.a == 2 RETURN d) "+
        "LET E = (FOR e IN UTUseIndexRangeBothInd FILTER e.a == 2 RETURN e) "+
        "LET F = (FOR f IN UTUseIndexRangeBothInd FILTER f.a == 2 RETURN f) "+
        "LET G = (FOR g IN UTUseIndexRangeBothInd FILTER g.a == 2 RETURN g) "+
        "LET H = (FOR h IN UTUseIndexRangeBothInd FILTER h.a == 2 RETURN h) "+
        "LET I = (FOR i IN UTUseIndexRangeBothInd FILTER i.a == 2 RETURN i) "+
        "LET J = (FOR j IN UTUseIndexRangeBothInd FILTER j.a == 2 RETURN j) "+
        "FOR k IN UTUseIndexRangeBothInd FILTER k.a == 2 "+
        "  RETURN {A:A, B:B, C:C, D:D, E:E, F:F, G:G, H:H, I:I, J:J, K:k}";

      var result = AQL_EXPLAIN(query, { }, paramEnabledAllPlans);
      assertTrue(result.plans.length < 40, query);
      result = AQL_EXPLAIN(query, { }, paramAllAllPlans);
      assertTrue(result.plans.length < 40, query);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that plan explosion does not happen
////////////////////////////////////////////////////////////////////////////////

    testRuleMakesAll : function () {
      var query =
        "LET A = (FOR a IN UTUseIndexRangeBothInd FILTER a.a == 2 RETURN a) "+
        "LET B = (FOR b IN UTUseIndexRangeBothInd FILTER b.a == 2 RETURN b) "+
        "LET C = (FOR c IN UTUseIndexRangeBothInd FILTER c.a == 2 RETURN c) "+
        "FOR k IN UTUseIndexRangeBothInd FILTER k.a == 2 "+
        "  RETURN {A:A, B:B, C:C, K:k}";

      var result = AQL_EXPLAIN(query, { }, paramEnabledAllPlans);
      assertTrue(result.plans.length === 1, query);
      result = AQL_EXPLAIN(query, { }, paramAllAllPlans);
      assertTrue(result.plans.length === 1, query);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleUseIndexRangeTester);

return jsunity.done();

