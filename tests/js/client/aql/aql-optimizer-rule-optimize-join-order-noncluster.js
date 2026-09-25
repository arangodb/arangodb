/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const waitForEstimatorSync = require("@arangodb/test-helper").waitForEstimatorSync;

function optimizeJoinOrderTestSuite () {
  const ruleName = "optimize-join-order";

  // Small, unindexed fixture: every distinct-value estimate on these
  // collections falls back to a default, so the rule's statistics are
  // never trustworthy here. Used to exercise the graph-detection path and
  // the defaulted-statistics guard, neither of which needs real statistics.
  const c1 = "UnitTestsOptimizeJoinOrder1";
  const c2 = "UnitTestsOptimizeJoinOrder2";

  // Indexed, size-asymmetric fixture: a persistent index on the join
  // attribute on both sides makes distinctValues() index-backed rather than
  // defaulted, and the size asymmetry is large enough that the cheaper order
  // clears the rule's improvement margin. This is the only fixture the rule
  // can genuinely reorder.
  const cnSmall = "UnitTestsOptimizeJoinOrderSmall";
  const cnLarge = "UnitTestsOptimizeJoinOrderLarge";
  const kSmallSize = 50;
  const kLargeSize = 5000;

  // The rule is disabled by default, so it must be requested explicitly.
  const paramForced   = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  const paramBase     = { optimizer: { rules: [ "-all" ] } };
  const paramDefault  = { };

  function rules(options, query) {
    return db._createStatement({ query, bindVars: {}, options }).explain().plan.rules;
  }

  // Compares two result sets as multisets, ignoring row order. Only safe for
  // queries whose result order is not otherwise guaranteed (i.e. no explicit
  // SORT) -- once the rule can genuinely reorder enumerations, row order is
  // no longer a property to assert on for those queries.
  function norm(arr) {
    return arr.map((x) => JSON.stringify(x)).sort();
  }

  return {

    setUpAll : function () {
      db._drop(c1);
      db._drop(c2);
      let col1 = db._create(c1);
      let col2 = db._create(c2);
      let docs1 = [], docs2 = [];
      for (let i = 0; i < 10; ++i) {
        docs1.push({ value: i });
        docs2.push({ value: i });
      }
      col1.insert(docs1);
      col2.insert(docs2);

      db._drop(cnSmall);
      db._drop(cnLarge);
      let colSmall = db._create(cnSmall);
      let colLarge = db._create(cnLarge);
      // Both sides need an index-backed distinct-value estimate for the
      // join attribute, or the estimator's statistics stay defaulted and
      // the rule declines to rewrite regardless of size asymmetry.
      colSmall.ensureIndex({ type: "persistent", fields: [ "joinKey" ] });
      colLarge.ensureIndex({ type: "persistent", fields: [ "joinKey" ] });

      let small = [];
      for (let i = 0; i < kSmallSize; ++i) {
        small.push({ joinKey: i });
      }
      colSmall.insert(small);

      // Every value in cnSmall.joinKey has kLargeSize / kSmallSize matches
      // in cnLarge, so the join is non-trivial (not empty, not a 1:1 match)
      // without letting either side's distinct count degenerate to 1.
      let large = [];
      for (let i = 0; i < kLargeSize; ++i) {
        large.push({ joinKey: i % kSmallSize });
      }
      colLarge.insert(large);

      // The rule's statistics come from index selectivity estimates, which
      // RocksDB updates asynchronously after a bulk insert. While they are
      // still stale, distinctValues() would default and the rule would
      // decline regardless of the size asymmetry -- so wait for them to
      // settle before any test relies on the rule actually firing.
      waitForEstimatorSync();
    },

    tearDownAll : function () {
      db._drop(c1);
      db._drop(c2);
      db._drop(cnSmall);
      db._drop(cnLarge);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rule is disabled by default and must not appear unless requested
////////////////////////////////////////////////////////////////////////////////

    testDisabledByDefault : function () {
      const query = `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value RETURN a.value`;
      // with the full default optimizer the rule must NOT be applied
      assertEqual(-1, rules(paramDefault, query).indexOf(ruleName), query);
      // and with -all it is not applied either
      assertEqual(-1, rules(paramBase, query).indexOf(ruleName), query);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rule finds a join graph but declines to rewrite when its
/// statistics are defaulted (no index on the join attribute). "Applied"
/// means the plan was actually changed, not merely that a graph was found,
/// so all of these -- including graphs with multiple components -- report
/// unapplied.
////////////////////////////////////////////////////////////////////////////////

    testDeclinesWhenStatisticsDefaulted : function () {
      const queries = [
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value RETURN a.value`,
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value FOR c IN ${c1} FILTER b.value == c.value RETURN a.value`,
        // disconnected join graph (two independent equijoins)
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value FOR c IN ${c1} FOR d IN ${c2} FILTER c.value == d.value RETURN a.value`,
      ];
      queries.forEach(function (query) {
        assertEqual(-1, rules(paramForced, query).indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rule does NOT fire when there is no equijoin to build a graph from
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      const queries = [
        // single enumeration, no join
        `FOR a IN ${c1} FILTER a.value == 1 RETURN a.value`,
        // cross product without an equijoin condition
        `FOR a IN ${c1} FOR b IN ${c2} RETURN a.value`,
        // join against a list, not two collections
        `FOR a IN ${c1} FOR b IN 1..10 FILTER a.value == b RETURN a.value`,
      ];
      queries.forEach(function (query) {
        assertEqual(-1, rules(paramForced, query).indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief rule IS reported applied when it genuinely reorders: real,
/// index-backed statistics on a sufficiently asymmetric join clear the
/// improvement margin. The written order (large collection outer, small
/// collection probed) is the expensive one.
////////////////////////////////////////////////////////////////////////////////

    testAppliedWhenReordered : function () {
      const query = `
        FOR a IN ${cnLarge}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            RETURN a.joinKey`;
      assertNotEqual(-1, rules(paramForced, query).indexOf(ruleName), query);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief the rule must not change query results when it is a genuine no-op
/// (defaulted statistics on the unindexed fixture).
////////////////////////////////////////////////////////////////////////////////

    testResultsInvariant : function () {
      const queries = [
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value RETURN [a.value, b.value]`,
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value FILTER a.value > 3 RETURN a.value`,
        `FOR a IN ${c1} FOR b IN ${c2} FILTER a.value == b.value FOR c IN ${c1} FILTER b.value == c.value RETURN a.value`,
      ];
      queries.forEach(function (query) {
        // -all vs -all + rule => plans are identical (rule is a no-op on
        // this fixture), so the result including ordering must be
        // byte-identical.
        const base   = db._query(query, {}, paramBase).toArray();
        const forced = db._query(query, {}, paramForced).toArray();
        assertEqual(base, forced, query);
        // and under the full default optimizer results are unchanged too
        // (compare as multisets, since full optimization may reorder rows
        // for reasons unrelated to this rule)
        const dflt = db._query(query, {}, paramDefault).toArray();
        assertEqual(norm(base), norm(dflt), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief once the rule can genuinely reorder, results must still be
/// identical to the written order. The written order here is deliberately
/// bad (large collection outer, small one probed), so this exercises an
/// actual rewrite, not a no-op. An explicit SORT makes the result order
/// deterministic regardless of enumeration order, so a strict comparison
/// (not a multiset one) is still the right check.
////////////////////////////////////////////////////////////////////////////////

    testResultsAreInvariantUnderReordering : function () {
      const query = `
        FOR a IN ${cnLarge}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            SORT a._key, b._key
            RETURN [a.joinKey, b.joinKey]`;

      const withRule = db._query(query, {}, {
        optimizer: { rules: ["+" + ruleName] }
      }).toArray();
      const withoutRule = db._query(query, {}, {
        optimizer: { rules: ["-" + ruleName] }
      }).toArray();

      assertEqual(withoutRule, withRule,
        "reordering must not change the result set");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief the payoff: after reordering, use-indexes must be able to turn the
/// inner enumeration into an IndexNode. Both collections carry a persistent
/// index on joinKey (each side needs index-backed statistics or the rule
/// declines), so whichever collection ends up inner becomes an IndexNode
/// either way -- asserting only that an IndexNode exists would pass whether
/// or not the rule actually reordered anything. What reordering buys is
/// *which* collection lands on which side: the small collection driving the
/// loop as a scan, the large one probed through the index, instead of the
/// reverse (see testResultsAreInvariantUnderReordering's proof).
////////////////////////////////////////////////////////////////////////////////

    testReorderingEnablesAnIndexLookup : function () {
      const query = `
        FOR a IN ${cnLarge}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            RETURN [a, b]`;

      // interchange-adjacent-enumerations is a pre-existing, independent rule
      // that also permutes adjacent FOR loops (by the engine's own cost
      // model, unrelated to this feature). It is disabled here so that any
      // reordering observed is attributable only to optimize-join-order --
      // otherwise this assertion would pass even if optimize-join-order did
      // nothing at all, because the older rule alone already finds the same
      // order on this fixture.
      const plan = db._createStatement({
        query,
        options: { optimizer: { rules: ["+" + ruleName, "-interchange-adjacent-enumerations"] } }
      }).explain().plan;

      const scanned = plan.nodes.filter(n => n.type === "EnumerateCollectionNode")
                                .map(n => n.collection);
      const probed  = plan.nodes.filter(n => n.type === "IndexNode")
                                .map(n => n.collection);

      assertEqual([cnSmall], scanned, JSON.stringify(plan.nodes.map(n => n.type)));
      assertEqual([cnLarge], probed, JSON.stringify(plan.nodes.map(n => n.type)));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief interchange-adjacent-enumerations is a brute-force alternative that
/// decides the very same thing this rule does, so once this rule has
/// actually reordered a join, interchange must not also run on the result
/// -- otherwise the two would compete over the same decision. This is the
/// only fixture (real, index-backed, size-asymmetric statistics) where the
/// rule demonstrably rewrites, which is what makes this assertion possible;
/// interchange is left at its default-enabled state (not disabled) so the
/// suppression itself is what is under test.
////////////////////////////////////////////////////////////////////////////////

    testInterchangeSuppressedWhenJoinOrderApplies : function () {
      const query = `
        FOR a IN ${cnLarge}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            RETURN [a, b]`;

      const stmt = db._createStatement({
        query, bindVars: {},
        options: { optimizer: { rules: ["+" + ruleName] } }
      });
      const explained = stmt.explain();

      // The rule has to have fired, or there is nothing to suppress and the
      // rest of this test would pass vacuously.
      assertNotEqual(-1, explained.plan.rules.indexOf(ruleName), query);

      // Do NOT assert on the absence of "interchange-adjacent-enumerations"
      // from plan.rules: that is vacuously true either way. This rule has
      // already produced the order interchange's own estimate prefers, so
      // even when interchange does run, the plan it permutes loses the cost
      // comparison and the winning plan carries no interchange marker.
      // Measured directly: with suppression removed, this query still
      // reports interchange absent from plan.rules while creating 2 plans
      // instead of 1.
      //
      // What suppression actually prevents is the n! fan-out, so assert on
      // that instead. plansCreated is 1 exactly when interchange never ran,
      // and rulesSkipped counts it as skipped.
      assertEqual(1, explained.stats.plansCreated, JSON.stringify(explained.stats));
      assertTrue(explained.stats.rulesSkipped >= 1, JSON.stringify(explained.stats));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief the other half of the guarantee: when this rule declines, opting
/// into it must not cost anything either. A plain cross product between
/// cnLarge and cnSmall gives optimize-join-order no equijoin to build a
/// graph from, so it declines exactly as in testRuleNoEffect. interchange-
/// adjacent-enumerations, left at its default-enabled state, must still be
/// free to reorder the same two enumerations by their generic (index-
/// independent) cost estimate, proving suppression did not fire.
////////////////////////////////////////////////////////////////////////////////

    testInterchangeStillFiresWhenJoinOrderDeclines : function () {
      const query = `
        FOR a IN ${cnLarge}
          FOR b IN ${cnSmall}
            RETURN [a, b]`;

      const planRules = rules({ optimizer: { rules: ["+" + ruleName] } }, query);
      assertEqual(-1, planRules.indexOf(ruleName), query);
      assertNotEqual(-1, planRules.indexOf("interchange-adjacent-enumerations"), query);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief a non-deterministic calculation between two joined enumerations
/// must keep the written order even though the estimator prefers the other
/// one: hoisting an enumeration above the calculation would change how many
/// times it is evaluated (|a| becomes |a|*|b|), and therefore change results.
////////////////////////////////////////////////////////////////////////////////

    testNonDeterministicCalculationBlocksReordering : function () {
      const query = `
        FOR a IN ${cnLarge}
          LET r = RAND()
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            RETURN [a.joinKey, r]`;

      // the rule must not report itself applied: it declines the whole run
      assertEqual(-1, rules(paramForced, query).indexOf(ruleName), query);

      // and the written FOR order must survive unchanged
      const plan = db._createStatement({ query, bindVars: {}, options: paramForced })
        .explain().plan;
      const enumerations = plan.nodes
        .filter(n => n.type === "EnumerateCollectionNode")
        .map(n => n.collection);
      assertEqual([cnLarge, cnSmall], enumerations, query);
    },

  };
}

jsunity.run(optimizeJoinOrderTestSuite);

return jsunity.done();
