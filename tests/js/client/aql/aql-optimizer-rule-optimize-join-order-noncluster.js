/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertNotEqual */

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
        optimizer: { rules: ["+optimize-join-order"] }
      }).toArray();
      const withoutRule = db._query(query, {}, {
        optimizer: { rules: ["-optimize-join-order"] }
      }).toArray();

      assertEqual(withoutRule, withRule,
        "reordering must not change the result set");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief the payoff: after reordering, use-indexes must be able to turn the
/// inner enumeration into an IndexNode. Asserting only the FOR order would
/// pass even if the reordering delivered nothing.
////////////////////////////////////////////////////////////////////////////////

    testReorderingEnablesAnIndexLookup : function () {
      const query = `
        FOR a IN ${cnLarge}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            RETURN [a, b]`;

      const plan = db._createStatement({
        query,
        options: { optimizer: { rules: ["+optimize-join-order"] } }
      }).explain().plan;

      const indexNodes = plan.nodes.filter(n => n.type === "IndexNode");
      assertTrue(indexNodes.length >= 1,
        "expected at least one IndexNode after index selection, got: " +
        JSON.stringify(plan.nodes.map(n => n.type)));
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
