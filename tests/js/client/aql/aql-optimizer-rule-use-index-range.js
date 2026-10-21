/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

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
// //////////////////////////////////////////////////////////////////////////////

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
  var collNames = ["NoInd", "SkipInd", "HashInd", "BothInd", "StrInd", "OtherInd"];
  var collNoInd;
  var collSkipInd;
  var collHashInd;
  var collBothInd;
  var collStrInd;
  var collOtherInd;
  var edgeCollName = collBaseName + "Edges";

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

    setUpAll : function () {
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
      // byte order and ICU collation disagree on this value vs "c"
      collNoInd.insert({"a":100, "s":"{"});
      collNoInd.insert({"a":101, "s":"a"});
      collSkipInd = colls[1];
      collSkipInd.ensureIndex({ type: "skiplist", fields: ["a"] });
      collHashInd = colls[2];
      collHashInd.ensureIndex({ type: "hash", fields: ["a"] });
      collBothInd = colls[3];
      collBothInd.ensureIndex({ type: "hash", fields: ["a"] });
      collBothInd.ensureIndex({ type: "skiplist", fields: ["a"] });
      collStrInd = colls[4];
      collStrInd.ensureIndex({ type: "skiplist", fields: ["s"] });
      // isolated from collSkipInd, other tests rely on its doc count for cost decisions
      collStrInd.insert({"a":100, "s":"{"});
      collStrInd.insert({"a":101, "s":"a"});
      collOtherInd = colls[5];
      // index doesn't cover s: use-indexes normalizes the condition but finds nothing to use
      collOtherInd.ensureIndex({ type: "skiplist", fields: ["a"] });
      collOtherInd.insert({"a":100, "s":"{"});
      collOtherInd.insert({"a":101, "s":"a"});

      internal.db._drop(edgeCollName);
      let edges = internal.db._createEdgeCollection(edgeCollName);
      edges.insert([{ _from: "v/0", _to: "v/1" }, { _from: "v/1", _to: "v/0" }]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      var n = collNames.map(function(x) { return collBaseName + x; });
      for (var i = 0; i < n.length; i++) {
        internal.db._drop(n[i]);
      }
      internal.db._drop(edgeCollName);
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
        var result = db._createStatement({query: query, bindVars:  { }, options:  paramNone}).explain();
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
        ["FOR i IN UTUseIndexRangeHashInd FILTER i.a >= 2 RETURN i", false],
        // a non-deterministic value can't be an index lookup value
        ["FOR i IN UTUseIndexRangeSkipInd FILTER i.a == FLOOR(RAND() * 10) RETURN i", true],
        ["FOR i IN UTUseIndexRangeHashInd FILTER i.a == FLOOR(RAND() * 10) RETURN i", true],
        ["FOR i IN UTUseIndexRangeSkipInd FILTER i.a >= FLOOR(RAND() * 10) RETURN i", true],
        ["FOR i IN UTUseIndexRangeSkipInd FILTER FLOOR(RAND() * 10) == i.a RETURN i", true],
        ["FOR i IN UTUseIndexRangeSkipInd FILTER i.a == NOOPT(FLOOR(RAND() * 10)) RETURN i", true],
        ["FOR i IN UTUseIndexRangeSkipInd FILTER i.a IN [FLOOR(RAND() * 10)] RETURN i", true],
        ["FOR e IN " + edgeCollName + " FILTER e._from == CONCAT('v/', FLOOR(RAND() * 2)) RETURN e", true],
        ["FOR i IN UTUseIndexRangeSkipInd FILTER i.a == NOOPT(2) RETURN i", true],
        ["FOR i IN UTUseIndexRangeHashInd FILTER i.a == NOOPT(2) RETURN i", true],
        ["FOR i IN UTUseIndexRangeSkipInd FILTER i.a == NOOPT(NOOPT(2)) RETURN i", true],
        ["FOR i IN UTUseIndexRangeSkipInd FILTER i.a == DOCUMENT('UTUseIndexRangeNoInd/x').a RETURN i", true],
        ["FOR i IN UTUseIndexRangeSkipInd FILTER i.a == COLLECTION_COUNT('UTUseIndexRangeNoInd') RETURN i", true],
        ["FOR e IN " + edgeCollName + " FILTER e._from == NOOPT('v/0') RETURN e", true],
      ];

      queries.forEach(function(query) {
        var result = db._createStatement({query: query[0], bindVars:  { }, options:  paramEnabled}).explain();
        if (query[1]) {
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
        "FOR i IN UTUseIndexRangeBothInd FILTER i.a > 2 RETURN i",
        // NOEVAL() is not computed in advance, but is deterministic
        "FOR i IN UTUseIndexRangeSkipInd FILTER i.a == NOEVAL(2) RETURN i",
        "FOR i IN UTUseIndexRangeHashInd FILTER i.a == NOEVAL(2) RETURN i",
        "FOR e IN " + edgeCollName + " FILTER e._from == NOEVAL('v/0') RETURN e",
      ];

      queries.forEach(function(query) {
        var result = db._createStatement({query: query, bindVars:  { }, options:  paramEnabled}).explain();
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
        var result = db._createStatement({query: query, bindVars:  { }, options:  paramAll}).explain();
        assertTrue(result.plan.rules.indexOf(ruleName) >= 0, query);
      });
    },

    testNonDeterministicPartNotInIndexCondition : function () {
      const query = "FOR i IN UTUseIndexRangeSkipInd " +
                    "FILTER i.a >= 2 && i.a < FLOOR(RAND() * 10) RETURN i";
      const plan = db._createStatement({query, bindVars: {}, options: paramEnabled}).explain().plan;
      const indexNodes = plan.nodes.filter((n) => n.type === "IndexNode");
      assertEqual(1, indexNodes.length, query);
      // the RAND() bound must stay in the filter, not in the index lookup
      assertFalse(JSON.stringify(indexNodes[0].condition).includes("RAND"), query);
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
        var result = db._createStatement({query: query, bindVars:  { }, options:  paramAll}).explain();
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
/// @brief test that string ranges are not falsely detected as impossible when byte order and ICU collation disagree
////////////////////////////////////////////////////////////////////////////////

    testImpossibleRangesNotFalselyDetectedForUtf8Strings : function () {
      var queries = [
        // single-element IN collapses to ==
        [ "FOR i IN UTUseIndexRangeNoInd FILTER i.s IN ['{'] && i.s < 'c' RETURN i.s", [ "{" ] ],
        // IN-list pruning needs 2+ elements to not collapse
        [ "FOR i IN UTUseIndexRangeNoInd FILTER i.s IN ['{', 'zzz'] && i.s < 'c' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeNoInd FILTER i.s == '{' && i.s < 'c' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeNoInd FILTER i.s == '{' && i.s <= 'c' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeNoInd FILTER i.s == 'a' && i.s > '{' RETURN i.s", [ "a" ] ],
        [ "FOR i IN UTUseIndexRangeNoInd FILTER i.s == 'a' && i.s >= '{' RETURN i.s", [ "a" ] ],
        // operand order must not matter
        [ "FOR i IN UTUseIndexRangeNoInd FILTER i.s < 'c' && i.s == '{' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeNoInd FILTER (i.s == '{' || i.s == 'zzz') && i.s < 'c' RETURN i.s", [ "{" ] ],
        // != paired with an ordering op also needs the collation rule
        [ "FOR i IN UTUseIndexRangeNoInd FILTER i.s < 'c' && i.s != '{' RETURN i.s", [ "a" ] ]
      ];

      queries.forEach(function(query) {
        var result = db._createStatement({query: query[0], bindVars: { }, options: paramAll}).explain();
        var foundNoResults = false;
        for (var t in result.plan.nodes) {
          if (result.plan.nodes[t].type === "NoResultsNode") {
            foundNoResults = true;
          }
        }
        assertFalse(foundNoResults, query);
        assertEqual(query[1], db._query(query[0]).toArray().sort(), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief same as above, but with an actual index on s, confirming the fix holds once an IndexNode is picked
////////////////////////////////////////////////////////////////////////////////

    testImpossibleRangesNotFalselyDetectedForUtf8StringsWithIndex : function () {
      const queries = [
        [ "FOR i IN UTUseIndexRangeStrInd FILTER i.s IN ['{'] && i.s < 'c' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeStrInd FILTER i.s IN ['{', 'zzz'] && i.s < 'c' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeStrInd FILTER i.s == '{' && i.s < 'c' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeStrInd FILTER (i.s == '{' || i.s == 'zzz') && i.s < 'c' RETURN i.s", [ "{" ] ]
      ];

      queries.forEach(function(query) {
        const result = db._createStatement({query: query[0], bindVars: { }, options: paramAll}).explain();
        let foundNoResults = false;
        let indexNodes = 0;
        result.plan.nodes.forEach(function(n) {
          if (n.type === "NoResultsNode") { foundNoResults = true; }
          if (n.type === "IndexNode") { indexNodes++; }
        });
        assertFalse(foundNoResults, query);
        assertEqual(1, indexNodes, query);
        assertEqual(query[1], db._query(query[0]).toArray().sort(), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief same as above, but the index doesn't cover s, so there is nothing for use-indexes to pick
////////////////////////////////////////////////////////////////////////////////

    testImpossibleRangesNotFalselyDetectedForUtf8StringsIndexNotCoveringAttribute : function () {
      const queries = [
        [ "FOR i IN UTUseIndexRangeOtherInd FILTER i.s IN ['{'] && i.s < 'c' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeOtherInd FILTER i.s IN ['{', 'zzz'] && i.s < 'c' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeOtherInd FILTER i.s == '{' && i.s < 'c' RETURN i.s", [ "{" ] ],
        [ "FOR i IN UTUseIndexRangeOtherInd FILTER (i.s == '{' || i.s == 'zzz') && i.s < 'c' RETURN i.s", [ "{" ] ]
      ];

      queries.forEach(function(query) {
        const result = db._createStatement({query: query[0], bindVars: { }, options: paramAll}).explain();
        let foundNoResults = false;
        let indexNodes = 0;
        result.plan.nodes.forEach(function(n) {
          if (n.type === "NoResultsNode") { foundNoResults = true; }
          if (n.type === "IndexNode") { indexNodes++; }
        });
        assertFalse(foundNoResults, query);
        // a-index can't serve a filter on s, so this falls back to a full scan, not an IndexNode
        assertEqual(0, indexNodes, query);
        assertEqual(query[1], db._query(query[0]).toArray().sort(), query);
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

      var result = db._createStatement({query: query, bindVars:  { }, options:  paramEnabledAllPlans}).explain();
      assertTrue(result.plans.length < 40, query);
      result = db._createStatement({query: query, bindVars:  { }, options:  paramAllAllPlans}).explain();
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

      var result = db._createStatement({query: query, bindVars:  { }, options:  paramEnabledAllPlans}).explain();
      assertTrue(result.plans.length === 1, query);
      result = db._createStatement({query: query, bindVars:  { }, options:  paramAllAllPlans}).explain();
      assertTrue(result.plans.length === 1, query);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleUseIndexRangeTester);

return jsunity.done();

