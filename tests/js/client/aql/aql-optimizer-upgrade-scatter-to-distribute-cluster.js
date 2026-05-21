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

function optimizerUpgradeScatterToDistributeSuite() {

  let col; // main collection, 3 shards
  let col_dsl; // distribute shards like the main collection
  let col_msk; // sharded, but unrelated collection with multiple shard keys
  let col_msk_dsl; // distribute shards like the collection with multiple shard keys
  let col_id;

  return {
    setUpAll: function () {
      db._drop("UnitTestsCollection_col_msk_dsl");
      db._drop("UnitTestsCollection_col_msk");
      db._drop("UnitTestsCollection_col_dsl");
      db._drop("UnitTestsCollection_col");
      db._drop("UnitTestsCollection_col_id");

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
    },

    tearDownAll: function () {
      db._drop("UnitTestsCollection_col_msk_dsl");
      db._drop("UnitTestsCollection_col_msk");
      db._drop("UnitTestsCollection_col_dsl");
      db._drop("UnitTestsCollection_col");
    },

    test_DefaultShardKey_Upgrade: function() {
      let query =
          `FOR doc1 IN ${col.name()}
            FOR doc2 IN ${col.name()}
             FILTER doc2._key == doc1.foreign_key
             RETURN [doc1, doc2]`;
      db._explain(query);
      print(JSON.stringify("shardKeys: " + db._collection(col_id.name()).properties().shardKeys, null, 2));
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
      print(JSON.stringify("shardKeys: " + db._collection(col_id.name()).properties().shardKeys, null, 2));
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
      print(JSON.stringify("shardKeys: " + db._collection(col_id.name()).properties().shardKeys, null, 2));
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query).toArray();
      assertNotEqual(result.length, col_id.count());
    },

    test_DefaultShardKey_Upgrade_id_3: function() {
      let query =
          `FOR doc1 IN ${col_id.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2.x == doc1.y
             RETURN [doc1, doc2]`;
      db._explain(query);
      print(JSON.stringify("shardKeys: " + db._collection(col_id.name()).properties().shardKeys, null, 2));
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query).toArray();
      assertNotEqual(result.length, col_id.count());
    },

    test_DefaultShardKey_Upgrade_id_4: function() {
      let query =
          `FOR doc1 IN ${col_id_key.name()}
            FOR doc2 IN ${col_id.name()}
             FILTER doc2._key == doc1._key
             RETURN [doc1, doc2]`;
      db._explain(query);
      print(JSON.stringify("shardKeys: " + db._collection(col_id_key.name()).properties().shardKeys, null, 2));
      print(JSON.stringify("shardKeys: " + db._collection(col_id.name()).properties().shardKeys, null, 2));
      let plan = db._createStatement({query: query}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query).toArray();
      assertEqual(result.length, col_id_key.count());
    }
  };
}

jsunity.run(optimizerUpgradeScatterToDistributeSuite);
return jsunity.done();
