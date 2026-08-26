/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue */

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
// The rule is not shard- or locality-aware: its cardinalities are cluster-wide
// totals while execution is per shard, and nothing prices scatter/gather. So
// this suite deliberately does NOT assert which order the rule picks, which
// collection ends up probed, or how many plans were created -- all of those are
// single-server properties, and asserting them here would pin behaviour the
// design explicitly does not promise.
//
// What must hold in a cluster is narrower and more important: enabling the rule
// must never change a query's results, must never produce an invalid plan, and
// must leave a query untouched when its statistics are not trustworthy. Those
// are shape-independent, so they survive scatter/gather.
function optimizeJoinOrderClusterTestSuite () {
  const ruleName = "optimize-join-order";

  // Indexed and size-asymmetric, mirroring the noncluster fixture: a
  // persistent index on the join attribute on both sides is what makes
  // distinctValues() index-backed rather than defaulted. Whether that is
  // enough for the rule to actually fire on a coordinator is exactly what is
  // unknown here, so nothing below depends on it firing.
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

////////////////////////////////////////////////////////////////////////////////
/// @brief the central guarantee: on a coordinator, enabling the rule must not
/// change what a join returns. This is the one property that holds whether the
/// rule reorders, declines, or reorders differently than it would on a single
/// server, so it is what a cluster test can honestly assert.
////////////////////////////////////////////////////////////////////////////////

    testResultsAreInvariantUnderTheRule : function () {
      const off = run(joinQuery, withoutRule);
      const on  = run(joinQuery, withRule);

      // A join of 5000 rows against 50 distinct keys: every large-side row
      // matches exactly one small-side row.
      assertEqual(kLargeSize, off.length, "fixture produced no join rows");
      assertEqual(norm(off), norm(on), joinQuery);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief a three-way join spans more shards and more scatter/gather
/// boundaries than a two-way one, and gives the rule a component it could
/// resequence. Results must still be identical.
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief two independent joins: the rule may reorder each component and may
/// resequence the components relative to each other. Cross products make the
/// row count large, so this uses small collections -- the point is that a
/// resequencing decision cannot change the result set.
////////////////////////////////////////////////////////////////////////////////

    testIndependentComponentsResultsAreInvariant : function () {
      const query = `
        FOR a IN ${cnSmall}
          FOR b IN ${cnSmall}
            FILTER a.joinKey == b.joinKey
            FOR c IN ${cnSmall}
              FOR d IN ${cnSmall}
                FILTER c.joinKey == d.joinKey
                LIMIT 500
                RETURN { a: a.joinKey, c: c.joinKey }`;

      const off = run(query, withoutRule);
      const on  = run(query, withRule);
      assertTrue(off.length > 0, "fixture produced no join rows");
      assertEqual(norm(off), norm(on), query);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief without an index on the join attributes the rule has no trustworthy
/// statistics, so it must decline -- on a coordinator as much as on a single
/// server. This is the guard that keeps the rule safe in a cluster it cannot
/// model, so it is worth pinning that it actually engages here.
////////////////////////////////////////////////////////////////////////////////

    testUnindexedJoinIsLeftAlone : function () {
      const query = `
        FOR a IN ${cnPlain}
          FOR b IN ${cnPlain}
            FILTER a.joinKey == b.joinKey
            LIMIT 100
            RETURN { a: a.joinKey }`;

      assertEqual(-1, explain(query, withRule).plan.rules.indexOf(ruleName), query);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief whatever the rule does to the plan, the result must remain a plan
/// the cluster can actually execute: the rewrite unlinks enumerations and
/// splices them back above the run's first dependency, which is where an
/// invalid plan would come from. Executing the query is the check -- an
/// unregistered variable or a broken dependency chain fails at instantiation.
////////////////////////////////////////////////////////////////////////////////

    testRewrittenPlanStaysExecutable : function () {
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
      // SORT + LIMIT makes this query's order deterministic, so unlike the
      // cases above these can be compared directly.
      assertEqual(off, on, query);
      assertEqual(10, on.length, query);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief the rule stays opt-in on a coordinator too. If it ever became
/// default-on without the shard-awareness work, this is what would catch it.
////////////////////////////////////////////////////////////////////////////////

    testDisabledByDefaultInCluster : function () {
      assertEqual(-1, explain(joinQuery, {}).plan.rules.indexOf(ruleName), joinQuery);
    },

  };
}

jsunity.run(optimizeJoinOrderClusterTestSuite);

return jsunity.done();
