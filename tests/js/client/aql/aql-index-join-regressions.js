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
// /
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");

const IndexJoinRegressionSuite = function () {
  const databaseName = "IndexJoinRegressionDB";
  const noJoin = {optimizer: {rules: ["-join-index-nodes"]}};

  const nodeTypes = (query, options = {}) =>
    db._createStatement({query, options}).explain().plan.nodes.map(n => n.type);

  const run = (query, options = {}) =>
    db._query(query, {}, options).toArray();

  const sorted = (rows) => rows.map(r => JSON.stringify(r)).sort();

  return {
    setUpAll: function () {
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);

      // constant key fields
      db._create("SA", {numberOfShards: 3, shardKeys: ["y"]});
      db._create("SB", {numberOfShards: 3, shardKeys: ["y"], distributeShardsLike: "SA"});
      for (const name of ["SA", "SB"]) {
        db[name].ensureIndex({type: "persistent", fields: ["k", "y"]});
      }
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({i, k: i % 37, y: i % 5});
      }
      db.SA.insert(docs);
      db.SB.insert(docs);

      // filters on stored values, several shards per DB server
      db._create("A", {numberOfShards: 5, shardKeys: ["x"]});
      db._create("B", {numberOfShards: 5, shardKeys: ["x"], distributeShardsLike: "A"});
      db.A.ensureIndex({type: "persistent", fields: ["x"], storedValues: ["s"]});
      db.B.ensureIndex({type: "persistent", fields: ["x"], storedValues: ["z"]});
      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({x: i % 400, y: i, s: i % 3, z: i % 5});
      }
      db.A.insert(docs);
      db.B.insert(docs);

      // join constants
      db._create("K1", {numberOfShards: 3, shardKeys: ["x"]});
      db._create("K2", {numberOfShards: 3, shardKeys: ["x"], distributeShardsLike: "K1"});
      db.K1.ensureIndex({type: "persistent", fields: ["x"]});
      db.K2.ensureIndex({type: "persistent", fields: ["y", "x"]});
      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({x: i % 300, y: i % 7});
      }
      db.K1.insert(docs);
      db.K2.insert(docs);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(databaseName);
    },

    testLimitOffsetWithConstantKeyFields: function () {
      const full = `FOR a IN SA FILTER a.k == 7 FOR b IN SB FILTER b.k == 7 AND b.y == a.y RETURN [a.i, b.i]`;
      const query = `FOR a IN SA FILTER a.k == 7 FOR b IN SB FILTER b.k == 7 AND b.y == a.y LIMIT 50, 1000 RETURN [a.i, b.i]`;
      assertNotEqual(-1, nodeTypes(query).indexOf("JoinNode"));

      const expected = sorted(run(full, noJoin));
      assertTrue(expected.length > 50);

      const actual = sorted(run(query));
      assertEqual(expected.length - 50, actual.length);
      const all = new Set(expected);
      actual.forEach(r => assertTrue(all.has(r), r));
    },

    testFilterOnStoredValuesAfterClone: function () {
      const query = `FOR a IN A SORT a.x FILTER a.s == 1 FOR b IN B FILTER b.x == a.x FILTER b.z == 3 RETURN [a, b.y]`;
      assertNotEqual(-1, nodeTypes(query).indexOf("JoinNode"));

      const expected = sorted(run(query, noJoin));
      assertTrue(expected.length > 0);
      assertEqual(expected, sorted(run(query)));
    },

    testForceOneShardAttributeValue: function () {
      if (!internal.isCluster()) {
        return;
      }
      const query = `FOR a IN A SORT a.x FOR b IN B FILTER b.x == a.x RETURN [a.x, a.y, b.y]`;
      const options = {forceOneShardAttributeValue: "1"};
      assertNotEqual(-1, nodeTypes(query, options).indexOf("JoinNode"));

      const expected = sorted(run(query, Object.assign({}, options, noJoin)));
      const unrestricted = run(query, noJoin);
      assertTrue(expected.length < unrestricted.length);
      assertEqual(expected, sorted(run(query, options)));
    },

    testDeterministicConstantUsesJoin: function () {
      const query = `FOR a IN K1 SORT a.x FOR b IN K2 FILTER b.y == 3 AND b.x == a.x RETURN [a._key, b._key]`;
      assertNotEqual(-1, nodeTypes(query).indexOf("JoinNode"));
      assertEqual(sorted(run(query, noJoin)), sorted(run(query)));
    },

    testNonDeterministicExpressionIsNoJoinConstant: function () {
      const query = `FOR a IN K1 SORT a.x FOR b IN K2 FILTER b.y == NOOPT(3) AND b.x == a.x RETURN [a._key, b._key]`;
      assertEqual(-1, nodeTypes(query).indexOf("JoinNode"));
      const expected = sorted(run(query.replace("NOOPT(3)", "3"), noJoin));
      assertTrue(expected.length > 0);
      assertEqual(expected, sorted(run(query)));
    },
  };
};

jsunity.run(IndexJoinRegressionSuite);
return jsunity.done();
