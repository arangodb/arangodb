/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual */

// TODO(listunov): Add disclaimer

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const explainer = require("@arangodb/aql/explainer");

function optimizerUpgradeScatterToDistributeSuite() {

  let col; // main collection, 3 shards
  let dsl_col; // distrubute shards like the main collection
  let rand_col; // sharded, but unrelated collection 

  return {
    setUpAll: function () {
      db._drop("UnitTestsCollection_rand_col");
      db._drop("UnitTestsCollection_dsl_col");
      db._drop("UnitTestsCollection_col");

      col = db._create("UnitTestsCollection_col", { numberOfShards: 3, shardKeys: ["_key", "mykey"] });
      dsl_col = db._create("UnitTestsCollection_dsl_col", { numberOfShards: 3, shardKeys: ["_key", "mykey"], distributeShardsLike: "UnitTestsCollection_col" });
      rand_col = db._create("UnitTestsCollection_rand_col", { numberOfShards: 3, shardKeys: ["_key", "mykey", "value"] });

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _key: "_key-" + i, mykey: "mykey-" + i, value: i });
      }
      col.insert(docs);
      dsl_col.insert(docs);
      rand_col.insert(docs);
    },

    tearDownAll: function () {
      db._drop("UnitTestsCollection_rand_col");
      db._drop("UnitTestsCollection_dsl_col");
      db._drop("UnitTestsCollection_col");
    },

    test_SanityCheck_AllShardKeys_DistributedShardsLike_SmartJoins: function () {
      const query = `
        FOR doc1 IN ${col.name()}
          FOR doc2 IN ${dsl_col.name()}
            FILTER (doc2._key == doc1._key
              AND doc2.mykey == doc1.mykey)
            RETURN [doc1, doc2]
      `;
      let plan = db._createStatement({query: query, bindVars:  {}}).explain().plan;
      assertTrue(plan.rules.includes("smart-joins"));
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_SanityCheck_PartialShardKeys_DistributedShardsLike_NoSmartJoins: function () {
      const query = `
        FOR doc1 IN ${col.name()}
          FOR doc2 IN ${dsl_col.name()}
            FILTER doc2._key == doc1._key
            RETURN [doc1, doc2]
      `;
      let plan = db._createStatement({query: query, bindVars:  {}}).explain().plan;
      assertFalse(plan.rules.includes("smart-joins"));
    },

    test_AllShardKeys_NoDistributedShardsLike_NoSmartJoins_Upgrade: function () {
      // TODO(listunov): Test multiple AND-branches
      const query = `
        FOR doc1 IN ${col.name()}
          FOR doc2 IN ${rand_col.name()}
            FILTER (doc2._key == doc1._key
              AND doc2.mykey == doc1.mykey
              AND doc2.value == doc1.value)
            RETURN [doc1, doc2]
      `;
      let plan = db._createStatement({query: query, bindVars:  {}}).explain().plan;
      console.error('-------------------------------------------------------');
      db._explain(query)
      console.error('-------------------------------------------------------');
      console.error(plan.rules);
      console.error('-------------------------------------------------------');
      assertFalse(plan.rules.includes("smart-joins"));
      assertTrue(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_AllShardKeys_NoDistributedShardsLike_NotAllShardKeys_NoUpgrade: function () {
      const query = `
        FOR doc1 IN ${col.name()}
          FOR doc2 IN ${rand_col.name()}
            FILTER doc2.mykey == doc1.mykey AND doc2.value == 5
            RETURN [doc1, doc2]
      `;
      let plan = db._createStatement({query: query, bindVars:  {}}).explain().plan;
      assertFalse(plan.rules.includes("smart-joins"));
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_AllShardKeys_NoDistributedShardsLike_NotAllConditionsContainShardKeys_NoUpgrade: function () {
      const query = `
        FOR doc1 IN ${col.name()}
          FOR doc2 IN ${rand_col.name()}
            FILTER (doc2._key == doc1._key
              AND doc2.mykey == doc1.mykey)
              OR doc2.value == 5
            RETURN [doc1, doc2]
      `;
      let plan = db._createStatement({query: query, bindVars:  {}}).explain().plan;
      assertFalse(plan.rules.includes("smart-joins"));
      assertFalse(plan.rules.includes("upgrade-scatter-to-distribute"));
    },

    test_PartialShardKeys_NoDistributedShardsLike_NoSmartJoins: function () {
      const query = `
        FOR doc1 IN ${col.name()}
          FOR doc2 IN ${rand_col.name()}
            FILTER doc2._key == doc1._key
            RETURN [doc1, doc2]
      `;
      let plan = db._createStatement({query: query, bindVars:  {}}).explain().plan;
      assertFalse(plan.rules.includes("smart-joins"));
    },

  };
}

jsunity.run(optimizerUpgradeScatterToDistributeSuite);
return jsunity.done();
