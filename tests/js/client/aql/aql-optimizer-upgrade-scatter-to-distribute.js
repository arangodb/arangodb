/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual */

// TODO(listunov): Add disclaimer

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const explainer = require("@arangodb/aql/explainer");

const options = {
  optimizer:{
    rules:[
      "-interchange-adjacent-enumerations"
    ]}
};

function optimizerUpgradeScatterToDistributeSuite() {

  let col; // main collection, 3 shards
  let col_dsl; // distribute shards like the main collection
  let col_msk; // sharded, but unrelated collection with multiple shard keys
  let col_msk_dsl; // distribute shards like the collection with multiple shard keys

  return {
    setUpAll: function () {
      db._drop("UnitTestsCollection_col_msk_dsl");
      db._drop("UnitTestsCollection_col_msk");
      db._drop("UnitTestsCollection_col_dsl");
      db._drop("UnitTestsCollection_col");

      col = db._create("UnitTestsCollection_col", { numberOfShards: 3});
      col_dsl = db._create("UnitTestsCollection_col_dsl", { numberOfShards: 3, distributeShardsLike: "UnitTestsCollection_col" });

      col_msk = db._create("UnitTestsCollection_col_msk", { numberOfShards: 3, shardKeys: ["shardKey", "shardKey2"] });
      col_msk_dsl = db._create("UnitTestsCollection_col_msk_dsl", { numberOfShards: 3, shardKeys: ["shardKey", "shardKey2"], distributeShardsLike: "UnitTestsCollection_col_msk" });

      let docs = [];
      let docs_no_key = [];
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
      }
      col.insert(docs);
      col_dsl.insert(docs);
      col_msk.insert(docs_no_key);
      col_msk_dsl.insert(docs_no_key);
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
      let plan = db._createStatement({query: query, options: options}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query, {}, {}, options).toArray();
      assertEqual(result.length, col.count());
    },

    test_MultipleShardKeys_Upgrade: function() {
      let query =
          `FOR doc1 IN ${col.name()}
            FOR doc2 IN ${col_msk.name()}
             FILTER doc2.shardKey == doc1.foreign_shardKey
              AND doc2.shardKey2 == doc1.foreign_shardKey2
             RETURN [doc1, doc2]`;
      let plan = db._createStatement({query: query, options: options}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query, {}, {}, options).toArray();
      assertEqual(result.length, col_msk.count());
    },


    test_NoAttributeAccess_Upgrade: function() {
      let query =
          `FOR doc1 IN ${col.name()}
            FOR doc2 IN ${col_msk.name()}
              FILTER doc2.shardKey == "shardKey-12"
               AND doc2.shardKey2 == "shardKey2-12"
               AND doc2.value == doc1.value
             RETURN [doc1, doc2]`;
      db._explain(query, {}, options);
      let plan = db._createStatement({query: query, options: options}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query, {}, {}, options).toArray();
      assertEqual(result.length, 1);
    },

    // TODO(listunov): Is this even possible ?
    test_MultipleAndBranches_AllShards_Upgrade: function() {
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
      db._explain(query, {}, options);
      let plan = db._createStatement({query: query, options: options}).explain().plan;
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
      let result = db._query(query, {}, {}, options).toArray();
      assertEqual(result.length, 2);
    },
  };
}

jsunity.run(optimizerUpgradeScatterToDistributeSuite);
return jsunity.done();
