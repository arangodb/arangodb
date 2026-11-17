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
const aqlfunctions = require("@arangodb/aql/functions");

const IndexJoinV8Suite = function () {
  const databaseName = "IndexJoinV8DB";
  const noJoin = {optimizer: {rules: ["-join-index-nodes"]}};

  const nodeTypes = (query) =>
    db._createStatement({query}).explain().plan.nodes.map(n => n.type);

  const run = (query, options = {}) =>
    db._query(query, {}, options).toArray();

  const sorted = (rows) => rows.map(r => JSON.stringify(r)).sort();

  return {
    setUpAll: function () {
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);

      db._create("K1", {numberOfShards: 3, shardKeys: ["x"]});
      db._create("K2", {numberOfShards: 3, shardKeys: ["x"], distributeShardsLike: "K1"});
      db.K1.ensureIndex({type: "persistent", fields: ["x"]});
      db.K2.ensureIndex({type: "persistent", fields: ["y", "x"]});
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({x: i % 300, y: i % 7});
      }
      db.K1.insert(docs);
      db.K2.insert(docs);

      aqlfunctions.register("UNITTESTS_JOIN::THREE", function () { return 3; });
    },

    tearDownAll: function () {
      try {
        aqlfunctions.unregister("UNITTESTS_JOIN::THREE");
      } catch (err) {}
      db._useDatabase("_system");
      db._dropDatabase(databaseName);
    },

    testUserFunctionIsNoJoinConstant: function () {
      const query = `FOR a IN K1 SORT a.x FOR b IN K2 FILTER b.y == UNITTESTS_JOIN::THREE() AND b.x == a.x RETURN [a._key, b._key]`;
      assertEqual(-1, nodeTypes(query).indexOf("JoinNode"));
      const expected = sorted(run(query.replace("UNITTESTS_JOIN::THREE()", "3"), noJoin));
      assertTrue(expected.length > 0);
      assertEqual(expected, sorted(run(query)));
    },
  };
};

jsunity.run(IndexJoinV8Suite);
return jsunity.done();
