/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual */
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
const isEnterprise = require("internal").isEnterprise();

function optimizerUpgradeScatterToDistributeSuite() {

  let col; // main collection, 3 shards
  let col_dsl; // distribute shards like the main collection
  let col_msk; // sharded, but unrelated collection with multiple shard keys
  let col_msk_dsl; // distribute shards like the collection with multiple shard keys
  let col_id;
  let col_id_key;
  let col_sat;
  let col_edge; // edges pointing into the satellite collection

  return {
    setUpAll: function () {
      db._drop("UnitTestsCollection_col_msk_dsl");
      db._drop("UnitTestsCollection_col_msk");
      db._drop("UnitTestsCollection_col_dsl");
      db._drop("UnitTestsCollection_col");
      db._drop("UnitTestsCollection_col_id");
      db._drop("UnitTestsCollection_col_id_key");
      db._drop("UnitTestsCollection_col_edge");
      db._drop("UnitTestsCollection_col_sat");

      col = db._create("UnitTestsCollection_col", { numberOfShards: 10});
      col_dsl = db._create("UnitTestsCollection_col_dsl", { numberOfShards: 10, distributeShardsLike: "UnitTestsCollection_col" });

      col_msk = db._create("UnitTestsCollection_col_msk", { numberOfShards: 3, shardKeys: ["shardKey", "shardKey2"] });
      col_msk_dsl = db._create("UnitTestsCollection_col_msk_dsl", { numberOfShards: 3, shardKeys: ["shardKey", "shardKey2"], distributeShardsLike: "UnitTestsCollection_col_msk" });

      col_id = db._create("UnitTestsCollection_col_id", { numberOfShards: 10});
      col_id_key = db._create("UnitTestsCollection_col_id_key", { numberOfShards: 3, shardKeys: ["_key"]});

      let docs = [];
      let docs_no_key = [];
      let docs_id = [];
      let docs_id_key = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({
          _key: "key-" + i,
          foreign_key : "key-" + i,
          shardKey: "shardKey-" + i,
          foreign_shardKey: "shardKey-" + i,
          shardKey2: "shardKey2-" + i,
          foreign_shardKey2: "shardKey2-" + i,
          value: i
        });
        docs_no_key.push({
          foreign_key : "key-" + i,
          shardKey: "shardKey-" + i,
          foreign_shardKey: "shardKey-" + i,
          shardKey2: "shardKey2-" + i,
          foreign_shardKey2: "shardKey2-" + i,
          value: i
        });
        docs_id.push({
          _key: "key-" + i,
          foreign_id: col_id.name()+ "/key-" + i,
          value: i
        });
        docs_id_key.push({
          _key: "key-" + i,
          foreign_id: col_id_key.name()+ "/key-" + i,
          value: i
        });  
      }
      col.insert(docs);
      col_dsl.insert(docs);
      col_msk.insert(docs_no_key);
      col_msk_dsl.insert(docs_no_key);
      col_id.insert(docs_id);
      col_id_key.insert(docs_id_key);

      if (isEnterprise) {
        col_sat = db._create("UnitTestsCollection_col_sat", {replicationFactor: "satellite"});
        col_edge = db._createEdgeCollection("UnitTestsCollection_col_edge", {numberOfShards: 3});
        let sat_docs = [];
        let edges = [];
        for (let i = 0; i < 10; ++i) {
          sat_docs.push({_key: "sat-" + i});
          edges.push({_key: "edge-" + i, _from: col_sat.name() + "/sat-" + i, _to: col_sat.name() + "/sat-" + i});
        }
        col_sat.insert(sat_docs);
        col_edge.insert(edges);
      }
    },

    tearDownAll: function () {
      db._drop("UnitTestsCollection_col_msk_dsl");
      db._drop("UnitTestsCollection_col_msk");
      db._drop("UnitTestsCollection_col_dsl");
      db._drop("UnitTestsCollection_col");
      db._drop("UnitTestsCollection_col_id");
      db._drop("UnitTestsCollection_col_id_key");
      db._drop("UnitTestsCollection_col_edge");
      db._drop("UnitTestsCollection_col_sat");
    },

    test_DefaultShardKey_Upgrade: function() {
      let query =
          `FOR doc1 IN ${col.name()}
            FOR doc2 IN ${col.name()}
             FILTER doc2._key == doc1.foreign_key
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query).toArray();
      assertEqual(result.length, col.count());
    },

    test_DefaultShardKey_FunctionOnOtherSide_Upgrade: function() {
      let query =
          `FOR i IN 0..${col.count()-1}
            FOR doc2 IN ${col.name()}
             FILTER doc2._key == CONCAT('key-', i)
             RETURN doc2`;
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query).toArray();
      assertEqual(result.length, col.count());
    },

    test_MultipleShardKeys_Upgrade: function() {
      let query =
          `FOR doc1 IN ${col.name()}
            FOR doc2 IN ${col_msk.name()}
             FILTER doc2.shardKey == doc1.foreign_shardKey
              AND doc2.shardKey2 == doc1.foreign_shardKey2
             RETURN [doc1, doc2]`;
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query).toArray();
      assertEqual(result.length, col_msk.count());
    },

    test_MultipleShardKeys_NotAllUsed_NoUpgrade: function() {
      let query =
          `FOR doc1 IN ${col.name()}
            FOR doc2 IN ${col_msk.name()}
             FILTER doc2.shardKey == doc1.foreign_shardKey
              AND doc2.value == doc1.value
             RETURN [doc1, doc2]`;
      let plan = db._createStatement({query: query}).explain().plan;
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_NoAttributeAccess_Upgrade: function() {
      let query =
          `FOR doc1 IN ${col.name()}
            FOR doc2 IN ${col_msk.name()}
              FILTER doc2.shardKey == "shardKey-12"
               AND doc2.shardKey2 == "shardKey2-12"
               AND doc2.value == doc1.value
             RETURN [doc1, doc2]`;
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query).toArray();
      assertEqual(result.length, 1);
    },

    test_MultipleAndBranches_AllShards_NoUpgrade: function() {
      let query =
          `FOR doc1 IN ${col.name()}
            FOR doc2 IN ${col_msk.name()}
             FILTER (doc2.shardKey == doc1.foreign_shardKey
              AND doc2.shardKey2 == doc1.foreign_shardKey2
              AND doc2.value == 12) 
              OR (doc2.shardKey == doc1.foreign_shardKey
              AND doc2.shardKey2 == doc1.foreign_shardKey2
              AND doc2.value == 15 AND doc2.foreign_key == "key-15")              
             RETURN [doc1, doc2]`;
      let plan = db._createStatement({query: query}).explain().plan;
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_1: function() {
      let query =
          `FOR doc1 IN ${col_id.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._id == doc1.foreign_id
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query).toArray();
      assertEqual(result.length, col_id.count());
    },

   test_DefaultShardKey_Upgrade_id_2: function() {
      let query =
          `FOR doc1 IN ${col_id.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._key == doc1._id
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_3: function() {
      let query =
          `FOR doc1 IN ${col_id.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2.x == doc1.y
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_4: function() {
      let query =
          `FOR doc1 IN ${col_id_key.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._key == doc1._key
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_5: function() {
      let query =
          `FOR doc1 IN ${col_id_key.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._id == doc1._id
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_6: function() {
      let query =
          `FOR doc1 IN ${col_id_key.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._id == doc1._key
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_7: function() {
      let query =
          `FOR doc1 IN ${col_id_key.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2.x == doc1.y
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_8: function() {
      let query =
          `FOR doc1 IN ${col_id_key.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2.x == doc1._id
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_9: function() {
      let query =
          `FOR doc1 IN ${col_id_key.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2.x == doc1._id
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_10: function() {
      let query =
          `FOR doc1 IN ${col_msk.name()}
            FOR doc2 IN ${col_msk_dsl.name()}
             FILTER doc2._key == doc1._key
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_11: function() {
      let query =
          `FOR doc1 IN ${col_msk.name()}
            FOR doc2 IN ${col_msk_dsl.name()}
             FILTER doc2._key == doc1._id
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_12: function() {
      let query =
          `FOR doc1 IN ${col_msk.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._key == doc1._key
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_13: function() {
      let query =
          `FOR doc1 IN ${col_msk.name()}
            FOR doc2 IN ${col_msk.name()}
             FILTER doc2._id == doc1._key
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_14: function() {
      let query =
          `FOR doc1 IN ${col_msk.name()}
            FOR doc2 IN ${col_msk.name()}
             FILTER doc2._id == doc1._id
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_15: function() {
      let query =
          `FOR doc1 IN ${col_msk.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._id == doc1._key
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_Upgrade_id_16: function() {
      let query =
          `FOR doc1 IN ${col_msk.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._key == doc1.shardKey
             RETURN [doc1, doc2]`;
      db._explain(query);
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_DefaultShardKey_ConstantAndId_RestrictToSingleShard: function() {
      const opts = {optimizer: {rules: ["-interchange-adjacent-enumerations"]}};
      let query =
          `FOR doc1 IN ${col_id.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._key == "key-42" AND doc2._id == doc1.foreign_id
             RETURN doc2._key`;
      let plan = db._createStatement({query, options: opts}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      assertTrue(plan.rules.includes("restrict-to-single-shard"));
      let result = db._query(query, {}, opts).toArray();
      assertEqual(["key-42"], result);
    },

    test_SatelliteJoinedIntoLaterSnippet_NoUpgrade: function() {
      if (!isEnterprise) {
        return;
      }
      let query =
          `FOR e1 IN ${col_edge.name()}
            FOR v IN ${col_sat.name()} FILTER v._id == e1._from
             FOR e2 IN ${col_edge.name()}
              RETURN [e1._key, v._key, e2._key]`;
      let plan = db._createStatement({query}).explain().plan;
      assertTrue(plan.rules.includes("remove-satellite-joins"));
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
      assertEqual([], plan.nodes.filter(n => n.type === "DistributeNode"));
      let result = db._query(query).toArray();
      assertEqual(col_edge.count() * col_edge.count(), result.length);
    }
  };
}

jsunity.run(optimizerUpgradeScatterToDistributeSuite);
return jsunity.done();
