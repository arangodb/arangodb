/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, limit optimizations
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var db = internal.db;
const disableSingleDocOp = { optimizer : { rules : [ "-optimize-cluster-single-document-operations"] } };

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlShardIdsTestSuite () {
  var collection = null;
  var cn = "UnitTestsShardIds";
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { numberOfShards: 4 });

      for (var i = 0; i < 100; ++i) {
        collection.save({ "value" : i });
      }

      var result = collection.count(true), sum = 0, shards = Object.keys(result);
      assertEqual(4, shards.length);
      shards.forEach(function(key) {
        sum += result[key];
      });
      assertEqual(100, sum);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief no restriction to a shard
////////////////////////////////////////////////////////////////////////////////

    testQueryUnrestricted : function () {
      var query = "FOR doc IN " + cn + " RETURN 1";

      var actual = AQL_EXECUTE(query);
      assertEqual(100, actual.json.length);
    },

    testQueryRestrictedToShards : function () {
      var count = collection.count(true);
      var shards = Object.keys(count);
      var query = "FOR doc IN " + cn + " RETURN 1";

      assertTrue(shards.length > 1);
      var sum = 0;
      shards.forEach(function(s) {
        var actual = AQL_EXECUTE(query, null, { shardIds: [ s ] });
        assertEqual(count[s], actual.json.length);
        sum += actual.json.length;
      });
      
      assertEqual(100, sum);
    }
  };
}

function ahuacatlShardIdsOptimizationTestSuite() {
  let collection = null;
  let collectionByKey = null;
  const cn = "UnitTestsShardIds";
  const cnKey = "UnitTestsShardIdsShardKey";
  const shardKey = "value";
  const numberOfShards = 9;

  const tearDown = () => {
    db._drop(cn);
    db._drop(cnKey);
  };
  
  const allNodesOfTypeAreRestrictedToShard = (nodes, typeName, col) => {
    let relevantNodes = nodes.filter(node => node.type === typeName);
    assertTrue(relevantNodes.length !== 0);
    return relevantNodes.every(node => col.shards().indexOf(node.restrictedTo) !== -1);
  };

  const hasDistributeNode = nodes => {
    return (nodes.filter(node => node.type === 'DistributeNode')).length > 0;
  };

  const validatePlan = (query, nodeType, c) => {
    const plan = AQL_EXPLAIN(query, {}, disableSingleDocOp).plan;
    assertFalse(hasDistributeNode(plan.nodes));
    assertTrue(allNodesOfTypeAreRestrictedToShard(plan.nodes, nodeType, c));
  };

  const dropIndexes = (col) => {
    let indexes = col.getIndexes();
    for (const idx of indexes) {
      if (idx.type !== "primary" && idx.type !== "edge") {
        col.dropIndex(idx.id);
      }
    }
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      tearDown();
      collection = internal.db._create(cn, { numberOfShards });
      collectionByKey = internal.db._create(cnKey, { numberOfShards, shardKeys: [shardKey] });
      let docs = [];

      for (let i = 0; i < 100; ++i) {
        docs.push({ "value" : i % 25, "joinValue" : i % 5 });
      }

      collection.save(docs);
      collectionByKey.save(docs);
      let fullCounts = collection.count(true);

      let shardsToCount = new Map();
      for (const [shard, count] of Object.entries(fullCounts)) {
        shardsToCount.set(shard, count); 
      }
      assertEqual(numberOfShards, shardsToCount.size);
      let sum = 0;
      for (let c of shardsToCount.values()) {
        sum += c;
      }
      assertEqual(100, sum);

      let shardsByKeyToCount = new Map();
      fullCounts = collectionByKey.count(true);
      for (const [shard, count] of Object.entries(fullCounts)) {
        shardsByKeyToCount.set(shard, count); 
      }
      assertEqual(numberOfShards, shardsByKeyToCount.size);
      sum = 0;
      for (let c of shardsByKeyToCount.values()) {
        sum += c;
      }
      assertEqual(100, sum);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown,

////////////////////////////////////////////////////////////////////////////////
/// @brief no restriction to a shard
////////////////////////////////////////////////////////////////////////////////

    testRestrictOnPrimaryKey : function () {
      let sample = [];
      for (let i = 0; i < 10; ++i) {
        sample.push(collection.any());
      }

      for (const doc of sample) {
        const queryKey = `FOR doc IN ${cn} FILTER doc._key == "${doc._key}" RETURN doc`;
        validatePlan(queryKey, "IndexNode", collection);
        let res = db._query(queryKey, {}, disableSingleDocOp).toArray();
        assertEqual(1, res.length);
        assertEqual(doc._key, res[0]._key);
        assertEqual(doc._rev, res[0]._rev);
      }
    },

    testRestrictOnShardKeyHashIndex : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 25; ++i) {
        const query = `FOR doc IN ${cnKey} FILTER doc.${shardKey} == ${i} RETURN doc`;
        validatePlan(query, "IndexNode", collectionByKey);
        let res = db._query(query).toArray();
        assertEqual(4, res.length);
        for (let doc of res) {
          assertEqual(i, doc.value);
        }
      }
    },

    testRestrictOnShardKeySkiplist : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureSkiplist(shardKey);

      for (let i = 0; i < 25; ++i) {
        const query = `FOR doc IN ${cnKey} FILTER doc.${shardKey} == ${i} RETURN doc`;
        validatePlan(query, "IndexNode", collectionByKey);
        let res = db._query(query).toArray();
        assertEqual(4, res.length);
        for (let doc of res) {
          assertEqual(i, doc.value);
        }
      }
    },

    testRestrictMultipleShards : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 25; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i}
            FOR joined IN ${cnKey}
              FILTER joined.${shardKey} == ${i % 5}
              FILTER joined.joinValue == doc.joinValue
            RETURN [doc, joined]
        `;
        validatePlan(query, "IndexNode", collectionByKey);
        let res = db._query(query).toArray();
        // we find 4 in first Loop, and 4 joins each
        assertEqual(16, res.length);
        for (let [doc, joined] of res) {
          assertEqual(i, doc.value);
          assertEqual(i % 5, joined.value);
          assertEqual(doc.joinValue, joined.joinValue);
        }
      }
    },

    /* Not supported yet
    testRestrictOnShardKeyNoIndex : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 25; ++i) {
        const query = `FOR doc IN ${cnKey} FILTER doc.${shardKey} == ${i} RETURN doc`;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);
        let res = db._query(query).toArray();
        assertEqual(4, res.length);
        for (let doc of res) {
          assertEqual(i, doc.value);
        }
      }
    }
    */
  };
};


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlShardIdsTestSuite);
jsunity.run(ahuacatlShardIdsOptimizationTestSuite);

return jsunity.done();
