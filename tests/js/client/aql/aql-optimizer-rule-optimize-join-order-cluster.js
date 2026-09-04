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

// Cluster counterpart to aql-optimizer-rule-optimize-join-order-noncluster.js.
//
// The rule runs before distribute-in-cluster and scatter-in-cluster and reads
// cluster-wide cardinalities, so the shard layout never enters its decision.
// That makes the order assertions below deterministic -- and also means the
// rule is not shard- or locality-aware, so they pin today's behaviour rather
// than an optimality promise.
function optimizeJoinOrderClusterTestSuite () {
  const ruleName = "optimize-join-order";

  // Indexed and size-asymmetric, mirroring the noncluster fixture: a
  // persistent index on the join attribute on both sides is what makes
  // distinctValues() index-backed rather than defaulted. Verified sufficient
  // on a coordinator -- counts and selectivity estimates resolve there, and
  // the rule does fire on this fixture.
  const cnSmall = "UnitTestsOptimizeJoinOrderClusterSmall";
  const cnLarge = "UnitTestsOptimizeJoinOrderClusterLarge";
  const kSmallSize = 50;
  const kLargeSize = 5000;
  const kShards = 3;

  // Unindexed, so every statistic falls back to a default and the rule must
  // decline regardless of shard layout.
  const cnPlain = "UnitTestsOptimizeJoinOrderClusterPlain";

  const withRule    = { optimizer: { rules: [ "+" + ruleName ] } };
  const withoutRule = { optimizer: { rules: [ "-" + ruleName ] } };

  function run(query, options) {
    return db._createStatement({ query, bindVars: {}, options }).execute().toArray();
  }

  function explain(query, options) {
    return db._createStatement({ query, bindVars: {}, options }).explain();
  }

  // Multiset comparison: the rule may legitimately change row order, so only
  // the contents are a property worth asserting.
  function norm(arr) {
    return arr.map((x) => JSON.stringify(x)).sort();
  }

  // The scan/probe sequence of a plan, as a comparable list. Node *types*
  // matter as much as order here: after use-indexes the inner side of a
  // reordered join becomes an IndexNode, which is the actual payoff.
  function joinSequence(query, options) {
    return explain(query, options).plan.nodes
      .filter((n) => n.type === "EnumerateCollectionNode" || n.type === "IndexNode")
      .map((n) => (n.type === "IndexNode" ? "IDX:" : "SCAN:") + n.collection);
  }

  const joinQuery = `
    FOR a IN ${cnLarge}
      FOR b IN ${cnSmall}
        FILTER a.joinKey == b.joinKey
        RETURN { a: a.joinKey, b: b.joinKey }`;

  return {

    setUpAll : function () {
      [cnSmall, cnLarge, cnPlain].forEach((c) => db._drop(c));

      const small = db._create(cnSmall, { numberOfShards: kShards });
      const large = db._create(cnLarge, { numberOfShards: kShards });
      const plain = db._create(cnPlain, { numberOfShards: kShards });

      small.ensureIndex({ type: "persistent", fields: [ "joinKey" ] });
      large.ensureIndex({ type: "persistent", fields: [ "joinKey" ] });

      let docs = [];
      for (let i = 0; i < kSmallSize; ++i) {
        docs.push({ joinKey: i });
      }
      small.insert(docs);

      docs = [];
      for (let i = 0; i < kLargeSize; ++i) {
        docs.push({ joinKey: i % kSmallSize });
      }
      large.insert(docs);
      plain.insert(docs);

      // Index selectivity estimates are updated asynchronously after a bulk
      // insert; while they are stale the rule would decline for a reason that
      // has nothing to do with the cluster.
      waitForEstimatorSync();
    },

    tearDownAll : function () {
      [cnSmall, cnLarge, cnPlain].forEach((c) => db._drop(c));
    },

    // Enabling the rule must not change what a join returns.
    testResultsAreInvariantUnderTheRule : function () {
      const off = run(joinQuery, withoutRule);
      const on  = run(joinQuery, withRule);

      // A join of 5000 rows against 50 distinct keys: every large-side row
      // matches exactly one small-side row.
      assertEqual(kLargeSize, off.length, "fixture produced no join rows");
      assertEqual(norm(off), norm(on), joinQuery);
    },

    // More shard boundaries than the two-way case.
    testThreeWayJoinResultsAreInvariant : function () {
      const query = `
        FOR a IN ${cnLarge}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            FOR c IN ${cnSmall}
              FILTER b.joinKey == c.joinKey
              RETURN { a: a.joinKey, c: c.joinKey }`;

      const off = run(query, withoutRule);
      const on  = run(query, withRule);
      assertTrue(off.length > 0, "fixture produced no join rows");
      assertEqual(norm(off), norm(on), query);
    },

    testIndependentComponentsResultsAreInvariant : function () {
    // Two components: a resequencing decision must not change the results.
      //
      // Deliberately no LIMIT. A LIMIT without a total order picks an
      // arbitrary subset, and *which* rows survive depends on the enumeration
      // order -- precisely what this rule changes -- so it would make the
      // invariant false by construction. (It did: this test failed in CI
      // with LIMIT 500 while passing locally, because the two shard layouts
      // surfaced different 500 rows.)
      const query = `
        FOR a IN ${cnSmall}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            FOR c IN ${cnSmall}
              FOR d IN ${cnSmall}
                FILTER c.joinKey == d.joinKey
                RETURN { a: a.joinKey, c: c.joinKey }`;

      const off = run(query, withoutRule);
      const on  = run(query, withRule);
      // joinKey is unique in cnSmall, so each of the two joins yields
      // kSmallSize rows and the independent components cross-multiply.
      assertEqual(kSmallSize * kSmallSize, off.length, "unexpected fixture size");
      assertEqual(norm(off), norm(on), query);
    },

    testUnindexedJoinIsLeftAlone : function () {
    // No index means no trustworthy statistics, so the rule must decline.
      const query = `
        FOR a IN ${cnPlain}
          FOR b IN ${cnPlain}
            FILTER a.joinKey == b.joinKey
            LIMIT 100
            RETURN { a: a.joinKey }`;

      assertEqual(-1, explain(query, withRule).plan.rules.indexOf(ruleName), query);
    },

    testRewrittenPlanStaysExecutable : function () {
    // The rewrite unlinks and re-splices enumerations; executing the query is
    // what catches an unregistered variable or a broken dependency chain.
      const query = `
        FOR a IN ${cnLarge}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            LET tag = CONCAT("k", b.joinKey)
            FILTER tag != ""
            SORT a.joinKey
            LIMIT 10
            RETURN { key: a.joinKey, tag }`;

      const off = run(query, withoutRule);
      const on  = run(query, withRule);
      // Comparable directly, but note SORT a.joinKey is *not* a total order
      // (cnLarge holds kLargeSize/kSmallSize rows per key). It is safe only
      // because the projection is a function of joinKey alone, so all tied
      // rows are identical. Adding a field that varies within a key would
      // make this non-deterministic -- add a tie-breaker to the SORT first.
      assertEqual(off, on, query);
      assertEqual(10, on.length, query);
    },

    testRuleAloneReordersTheJoin : function () {
    // interchange reaches the same order on this fixture by its own cost model,
    // so isolating with "-all" is what attributes the flip to this rule.
    //
    // These pin a shard-blind decision; a shard-aware estimator should change
    // them.
      // Written large-first; the cheaper order scans the small side.
      const baseline = joinSequence(joinQuery, { optimizer: { rules: [ "-all" ] } });
      assertEqual([ "SCAN:" + cnLarge, "SCAN:" + cnSmall ], baseline,
                  "baseline must keep the written order");

      const reordered = joinSequence(joinQuery,
        { optimizer: { rules: [ "-all", "+" + ruleName ] } });
      assertEqual([ "SCAN:" + cnSmall, "SCAN:" + cnLarge ], reordered,
                  "the rule alone must flip the join on a coordinator");

      assertNotEqual(-1,
        explain(joinQuery, { optimizer: { rules: [ "-all", "+" + ruleName ] } })
          .plan.rules.indexOf(ruleName), joinQuery);
    },

    testReorderedInnerSideUsesItsIndex : function () {
    // The payoff: use-indexes turns the reordered inner side into an IndexNode.
      // Returns whole documents deliberately. joinQuery projects only
      // joinKey, which the persistent index covers, so use-indexes turns
      // *both* sides into IndexNodes there and scan-vs-probe becomes
      // indistinguishable. Needing the full document forces the outer side
      // to be a real collection scan, which is what makes "small scanned,
      // large probed" a meaningful assertion.
      const query = `
        FOR a IN ${cnLarge}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            RETURN [ a, b ]`;

      const seq = joinSequence(query, {
        optimizer: { rules: [ "+" + ruleName, "-interchange-adjacent-enumerations" ] }
      });
      assertEqual([ "SCAN:" + cnSmall, "IDX:" + cnLarge ], seq,
                  "small side scanned, large side probed through its index");
    },

    testDisabledByDefaultInCluster : function () {
    // Still opt-in on a coordinator.
      assertEqual(-1, explain(joinQuery, {}).plan.rules.indexOf(ruleName), joinQuery);
    },

  };
}

jsunity.run(optimizeJoinOrderClusterTestSuite);

return jsunity.done();
