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

// The primary index returns keys in byte order, persistent indexes in
// VelocyPack (ICU) order. These keys sort differently in the two orders.
const keys = ["a", "B", "c", "D", "_x", "-y", "A1", "a1", "aa", "AB", "Ab",
              "b-", "b_", "Z", "z", "0", "9:", "@m", "b", "ba"];

const IndexJoinKeyOrderSuite = function () {
  const databaseName = "IndexJoinKeyOrderDB";
  const noJoin = {optimizer: {rules: ["-join-index-nodes"]}};
  const generic = {joinStrategyType: "generic"};

  const nodeTypes = (query) =>
    db._createStatement({query}).explain().plan.nodes.map(n => n.type);

  const run = (query, options = {}) =>
    db._query(query, {}, options).toArray();

  const sorted = (rows) => rows.map(r => JSON.stringify(r)).sort();

  const assertJoin = (query, expectJoinNode) => {
    assertEqual(expectJoinNode, nodeTypes(query).indexOf("JoinNode") !== -1, query);
    const expected = sorted(run(query, noJoin));
    assertTrue(expected.length > 0);
    assertEqual(expected, sorted(run(query)));
    if (expectJoinNode) {
      assertEqual(expected, sorted(run(query, generic)));
    }
  };

  return {
    setUpAll: function () {
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);

      // joined on _key
      db._create("PA", {numberOfShards: 3});
      db._create("PB", {numberOfShards: 3, distributeShardsLike: "PA"});
      db._create("PC", {numberOfShards: 3, distributeShardsLike: "PA"});
      db.PA.insert(keys.map(k => ({_key: k})));
      db.PB.insert(keys.filter((k, i) => i % 2 === 0).map(k => ({_key: k})));
      db.PC.insert(keys.filter((k, i) => i % 3 !== 0).map(k => ({_key: k})));

      // joined on k
      db._create("KA", {numberOfShards: 3, shardKeys: ["k"]});
      db._create("KB", {numberOfShards: 3, shardKeys: ["k"], distributeShardsLike: "KA"});
      db.KA.ensureIndex({type: "persistent", fields: ["k"]});
      db.KB.ensureIndex({type: "persistent", fields: ["k"]});
      db.KA.insert(keys.map(k => ({k})));
      db.KB.insert(keys.concat(keys).map(k => ({k})));

      // vertices and edges
      db._create("V", {numberOfShards: 3});
      db._createEdgeCollection("E", {numberOfShards: 3, distributeShardsLike: "V"});
      db.V.insert(keys.map(k => ({_key: k, ref: `V/${k}`})));
      db.V.ensureIndex({type: "persistent", fields: ["ref"]});
      db.E.insert(keys.map((k, i) => ({_key: k, _from: `V/${k}`, _to: `V/${keys[(i + 1) % keys.length]}`})));
      db.E.ensureIndex({type: "persistent", fields: ["_from"], name: "fromIdx"});
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(databaseName);
    },

    testPrimaryIndexWithPrimaryIndex: function () {
      assertJoin(`FOR a IN PA FOR b IN PB FILTER b._key == a._key RETURN a._key`, true);
    },

    testPrimaryIndexThreeWay: function () {
      assertJoin(`FOR a IN PA FOR b IN PB FOR c IN PC FILTER b._key == a._key FILTER c._key == a._key RETURN a._key`, true);
    },

    testPrimaryIndexWithPersistentIndex: function () {
      assertJoin(`FOR a IN PA FOR b IN KB FILTER b.k == a._key RETURN [a._key, b.k]`, false);
      assertJoin(`FOR b IN KB SORT b.k FOR a IN PA FILTER a._key == b.k RETURN [a._key, b.k]`, false);
    },

    testPersistentIndexWithPersistentIndex: function () {
      assertJoin(`FOR a IN KA SORT a.k FOR b IN KB FILTER b.k == a.k RETURN [a.k, b.k]`, true);
    },

    testEdgesWithVerticesOnKey: function () {
      assertJoin(`FOR v IN V FOR e IN E FILTER e._key == v._key RETURN [v._key, e._to]`, true);
    },

    testEdgesWithVerticesOnPersistentIndexes: function () {
      // both legs use persistent indexes, so a JoinNode is possible on a
      // single server
      const query = `FOR v IN V SORT v.ref FOR e IN E OPTIONS {indexHint: "fromIdx", forceIndexHint: true} FILTER e._from == v.ref RETURN [v._key, e._to]`;
      if (require("internal").isCluster()) {
        const expected = sorted(run(query, noJoin));
        assertTrue(expected.length > 0);
        assertEqual(expected, sorted(run(query)));
      } else {
        assertJoin(query, true);
      }
    },

    testEdgeIndexIsNoJoinLeg: function () {
      // the edge index is not sorted
      assertJoin(`FOR v IN V SORT v._key FOR e IN E OPTIONS {indexHint: "edge", forceIndexHint: true} FILTER e._from == v._id RETURN [v._key, e._to]`, false);
    },
  };
};

jsunity.run(IndexJoinKeyOrderSuite);
return jsunity.done();
