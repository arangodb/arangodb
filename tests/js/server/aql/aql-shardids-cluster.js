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
const disableSingleDocOp = {
  optimizer: {
    rules: [ "-optimize-cluster-single-document-operations"]
  }
};
const disableSingleShard = {
  optimizer: {
    rules: [ "-restrict-to-single-shard"]
  }
};

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

    setUpAll : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { numberOfShards: 4 });

      let docs = [];
      for (var i = 0; i < 100; ++i) {
        docs.push({ "value" : i });
      }
      collection.insert(docs);

      var result = collection.count(true);
      var sum = 0;
      var shards = Object.keys(result);
      assertEqual(4, shards.length);
      shards.forEach(function(key) {
        sum += result[key];
      });
      assertEqual(100, sum);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
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
  let collectionByKey1 = null;
  let collectionByKey2 = null;
  const cn = "UnitTestsShardIds";
  const cnKey = "UnitTestsShardIdsShardKey";
  const cnKey1 = "UnitTestsShardIdsShardKey1";
  const cnKey2 = "UnitTestsShardIdsShardKey2";
  const shardKey = "value";
  const shardKey1 = "value";
  const shardKey2 = "value";
  const extraKey = "extra";
  const numberOfShards = 9;

  const tearDown = () => {
    db._drop(cn);
    db._drop(cnKey);
    db._drop(cnKey1);
    db._drop(cnKey2);
  };

  const allNodesOfTypeAreRestrictedToShard = (nodes, typeName, col) => {
    let relevantNodes = nodes.filter( node =>
        node.type === typeName && node.collection === col.name());
    assertTrue(relevantNodes.length !== 0);
    return relevantNodes.every(
        node => col.shards().indexOf(node.restrictedTo) !== -1);
  };

  const hasDistributeNode = nodes => {
    return (nodes.filter(node => node.type === 'DistributeNode')).length > 0;
  };

  const validatePlan = (query, nodeType, c) => {
    const plan = AQL_EXPLAIN(query, {}, disableSingleDocOp).plan;
    assertFalse(hasDistributeNode(plan.nodes));
    const allRestricted = allNodesOfTypeAreRestrictedToShard(plan.nodes,
                                                             nodeType, c);
    if (!allRestricted) {
      db._explain(query, {}, disableSingleDocOp);
    }
    assertTrue(allRestricted);
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

    setUpAll : function () {
      tearDown();
      collection = internal.db._create(cn, { numberOfShards });
      collectionByKey = internal.db._create(
          cnKey, { numberOfShards, shardKeys: [shardKey] });
      collectionByKey1 = internal.db._create(
          cnKey1, { numberOfShards, shardKeys: [shardKey1] });
      collectionByKey2 = internal.db._create(
          cnKey2, { numberOfShards, shardKeys: [shardKey2] });
      let docs = [];

      for (let i = 0; i < 100; ++i) {
        docs.push({ "value" : i % 25, "joinValue" : i % 5, "extra": true });
      }

      collection.save(docs);
      collectionByKey.save(docs);
      collectionByKey1.save(docs);
      collectionByKey2.save(docs);
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

    tearDownAll() {
      tearDown();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief no restriction to a shard
////////////////////////////////////////////////////////////////////////////////

    testRestrictOnPrimaryKey : function () {
      let sample = [];
      for (let i = 0; i < 10; ++i) {
        sample.push(collection.any());
      }

      for (const doc of sample) {
        const queryKey = `
          FOR doc IN ${cn}
            FILTER doc._key == "${doc._key}"
            RETURN doc`;
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
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i}
            RETURN doc`;
        validatePlan(query, "IndexNode", collectionByKey);
        let res = db._query(query, {}, disableSingleDocOp).toArray();
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
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i}
            RETURN doc`;
        validatePlan(query, "IndexNode", collectionByKey);
        let res = db._query(query, {}, disableSingleDocOp).toArray();
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

        let res = db._query(query, {}, disableSingleDocOp).toArray();
        // we find 4 in first Loop, and 4 joins each
        assertEqual(16, res.length);
        for (let [doc, joined] of res) {
          assertEqual(i, doc.value);
          assertEqual(i % 5, joined.value);
          assertEqual(doc.joinValue, joined.joinValue);
        }
      }
    },

    testRestrictMultipleShardsStringKeys : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 25; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == "key${i}"
            FOR joined IN ${cnKey}
              FILTER joined.${shardKey} == "key${i}"
              FILTER joined.joinValue == doc.joinValue
            RETURN [doc, joined]
        `;

        validatePlan(query, "IndexNode", collectionByKey);
      }
    },

    testRestrictOnPrimaryAndShardKeyNoIndex : function () {
      dropIndexes(collectionByKey);
      let sample = [];
      for (let i = 0; i < 10; ++i) {
        sample.push(collectionByKey.any());
      }

      for (const doc of sample) {
        const queryKey = `
          FOR doc IN ${cnKey}
            FILTER doc._key == "${doc._key}"
            FILTER doc.${shardKey} == ${doc[shardKey]}
            RETURN doc`;
        validatePlan(queryKey, "IndexNode", collectionByKey);
        let res = db._query(queryKey, {}, disableSingleDocOp).toArray();
        assertEqual(1, res.length);
        assertEqual(doc._key, res[0]._key);
        assertEqual(doc._rev, res[0]._rev);
      }
    },

    testRestrictOnPrimaryAndShardKeyJoinNoIndex : function () {
      dropIndexes(collectionByKey);
      let sample = [];
      for (let i = 0; i < 10; ++i) {
        sample.push(collectionByKey.any());
      }

      for (const doc of sample) {
        const queryKey = `
          FOR doc IN ${cnKey}
            FILTER doc._key == "${doc._key}"
            FILTER doc.${shardKey} == ${doc[shardKey]}
            FOR joined IN ${cnKey}
              FILTER joined._key == "${doc._key}"
              FILTER joined.${shardKey} == ${doc[shardKey]}
            RETURN [doc, joined]`;
        validatePlan(queryKey, "IndexNode", collectionByKey);
        let res = db._query(queryKey, {}, disableSingleDocOp).toArray();
        assertEqual(1, res.length);
        assertEqual(doc._key, res[0][0]._key);
        assertEqual(doc._rev, res[0][0]._rev);
        assertEqual(doc._key, res[0][1]._key);
        assertEqual(doc._rev, res[0][1]._rev);
      }
    },

    testRestrictOnShardKeyNoIndex : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 25; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i}
            RETURN doc`;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);
        let res = db._query(query, {}, disableSingleDocOp).toArray();
        assertEqual(4, res.length);
        for (let doc of res) {
          assertEqual(i, doc.value);
        }
      }
    },

    testRestrictMultipleShardsNoIndex : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 25; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i}
            FOR joined IN ${cnKey}
              FILTER joined.${shardKey} == ${i % 5}
              FILTER joined.joinValue == doc.joinValue
            RETURN [doc, joined]
        `;

        let res = db._query(query, {}, disableSingleDocOp).toArray();
        // we find 4 in first Loop, and 4 joins each
        assertEqual(16, res.length);
        for (let [doc, joined] of res) {
          assertEqual(i, doc.value);
          assertEqual(i % 5, joined.value);
          assertEqual(doc.joinValue, joined.joinValue);
        }
      }
    },

    testRestrictMultipleShardsStringKeysNoIndex : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 25; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == "key${i}"
            FOR joined IN ${cnKey}
              FILTER joined.${shardKey} == "key${i}"
              FILTER joined.joinValue == doc.joinValue
            RETURN [doc, joined]
        `;

        validatePlan(query, "EnumerateCollectionNode", collectionByKey);
      }
    },

    testMultipleKeysSameFilter: function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i} && doc.${extraKey} == true
            RETURN doc
        `;
        validatePlan(query, "IndexNode", collectionByKey);

        let res = db._query(query, {}, disableSingleDocOp).toArray();
        assertEqual(4, res.length);
        for (let doc of res) {
          assertTrue(i === doc.value);
          assertTrue(true === doc.extra);
        }
      }
    },

    testMultipleKeysSameFilterNoIndex: function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i} && doc.${extraKey} == true
            RETURN doc
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let res = db._query(query, {}, disableSingleDocOp).toArray();
        assertEqual(4, res.length);
        for (let doc of res) {
          assertTrue(i === doc.value);
          assertTrue(true === doc.extra);
        }
      }
    },

    testMultipleKeysDifferentFilter: function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i}
            FILTER doc.${extraKey} == true
            RETURN doc
        `;
        validatePlan(query, "IndexNode", collectionByKey);

        let res = db._query(query, {}, disableSingleDocOp).toArray();
        assertEqual(4, res.length);
        for (let doc of res) {
          assertTrue(i === doc.value);
          assertTrue(true === doc.extra);
        }
      }
    },

    testMultipleKeysDifferentFilterNoIndex: function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i}
            FILTER doc.${extraKey} == true
            RETURN doc
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let res = db._query(query, {}, disableSingleDocOp).toArray();
        assertEqual(4, res.length);
        for (let doc of res) {
          assertTrue(i === doc.value);
          assertTrue(true === doc.extra);
        }
      }
    },

    testMultipleShardsOr : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i} || doc.${shardKey} == ${i+1}
            RETURN doc
        `;
        // No restriction yet
        //validatePlan(query, "IndexNode", collectionByKey);

        let res = db._query(query, {}, disableSingleDocOp).toArray();
        assertEqual(8, res.length);
        for (let doc of res) {
          assertTrue(i === doc.value || i + 1 === doc.value);
        }
      }
    },

    testMultipleShardsOrNoIndex : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc IN ${cnKey}
            FILTER doc.${shardKey} == ${i} || doc.${shardKey} == ${i+1}
            RETURN doc
        `;
        // Multi-shard not implemented yet
        // validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let res = db._query(query, {}, disableSingleDocOp).toArray();
        assertEqual(8, res.length);
        for (let doc of res) {
          assertTrue(i === doc.value || i + 1 === doc.value);
        }
      }
    },

    testInnerOuterSame1 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
        }
      }
    },

    testInnerOuterSameIndex1 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
        }
      }
    },

    testInnerOuterSame2 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i+1}
              RETURN [doc1, doc2]
        `;
        // Multi-shard not implemented yet
        // validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i + 1 === doc2.value);
        }
      }
    },

    testInnerOuterSameIndex2 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i+1}
              RETURN [doc1, doc2]
        `;
        // Multi-shard not implemented yet
        // validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i + 1 === doc2.value);
        }
      }
    },

    testInnerOuterSame3 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSameIndex3 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSame4 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              RETURN doc1
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let doc of results) {
          assertTrue(i === doc.value);
        }
      }
    },

    testInnerOuterSameIndex4 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              RETURN doc1
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let doc of results) {
          assertTrue(i === doc.value);
        }
      }
    },

    testInnerOuterSame5 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSameIndex5 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleShard);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSame6 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSameIndex6 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            FOR doc2 IN ${cnKey}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSame7 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            RETURN NEW
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 4);
        let results = raw.toArray();
        assertEqual(4, results.length);
        for (let doc of results) {
          assertTrue(i === doc.value);
          assertEqual(doc.test, 1);
        }
      }
    },

    testInnerOuterSameIndex7 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            RETURN NEW
        `;
        validatePlan(query, "IndexNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 4);
        let results = raw.toArray();
        assertEqual(4, results.length);
        for (let doc of results) {
          assertTrue(i === doc.value);
          assertEqual(doc.test, 1);
        }
      }
    },

    testInnerOuterSame8 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc1.${shardKey} == ${i}
              FILTER doc2.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
        }
      }
    },

    testInnerOuterSameIndex8 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc1.${shardKey} == ${i}
              FILTER doc2.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
        }
      }
    },

    testInnerOuterSame9 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc1.${shardKey} == ${i}
              FILTER doc2.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSameIndex9 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc1.${shardKey} == ${i}
              FILTER doc2.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSame10 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc1.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
        }
      }
    },

    testInnerOuterSameIndex10 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc1.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
        }
      }
    },

    testInnerOuterSame11 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc1.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSameIndex11 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc1.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleShard);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSame12 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSameIndex12 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            FILTER doc1.${shardKey} == ${i}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSame13 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              FILTER doc1.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
        }
      }
    },

    testInnerOuterSameIndex13 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              FILTER doc1.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
        }
      }
    },

    testInnerOuterSame14 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              FILTER doc1.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSameIndex14 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              FILTER doc1.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSame15 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc2[shardKey]);
        }
      }
    },

    testInnerOuterSameIndex15 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              RETURN [doc1, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc2[shardKey]);
        }
      }
    },

    testInnerOuterSame16 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              FILTER NEW.${shardKey} == ${i}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleShard);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSameIndex16 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              FILTER NEW.${shardKey} == ${i}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleShard);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey]);
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSame17 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSameIndex17 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            FOR doc2 IN ${cnKey}
              FILTER doc2.${shardKey} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey}
              RETURN [NEW, doc2]
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc2[shardKey]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterSame18 : function () {
      dropIndexes(collectionByKey);

      const i = 18;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER NEW.${shardKey} == ${i}
            FILTER doc2.${shardKey} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleShard);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(16, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey]);
        assertTrue(i === doc2[shardKey]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSameIndex18 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      const i = 18;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER NEW.${shardKey} == ${i}
            FILTER doc2.${shardKey} == ${i}
            RETURN [NEW, doc2]
      `;
        let raw = db._query(query, {}, disableSingleShard);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(16, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey]);
        assertTrue(i === doc2[shardKey]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSame19 : function () {
      dropIndexes(collectionByKey);

      const i = 19;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER NEW.${shardKey} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(400, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSameIndex19 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      const i = 19;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER NEW.${shardKey} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(400, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSame20 : function () {
      dropIndexes(collectionByKey);

      const i = 20;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER doc2.${shardKey} == ${i}
            FILTER NEW.${shardKey} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(16, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey]);
        assertTrue(i === doc2[shardKey]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSameIndex20 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      const i = 20;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER doc2.${shardKey} == ${i}
            FILTER NEW.${shardKey} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(16, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey]);
        assertTrue(i === doc2[shardKey]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSame21 : function () {
      dropIndexes(collectionByKey);

      const i = 21;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER doc2.${shardKey} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(400, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc2[shardKey]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSameIndex21 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      const i = 21;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER doc2.${shardKey} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(400, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc2[shardKey]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSame22 : function () {
      dropIndexes(collectionByKey);

      const i = 22;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(10000, results.length);
      for (let [doc1, doc2] of results) {
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSameIndex22 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      const i = 22;
      const query = `
        FOR doc1 IN ${cnKey}
          FOR doc2 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(10000, results.length);
      for (let [doc1, doc2] of results) {
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterSame23 : function () {
      dropIndexes(collectionByKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER NEW.${shardKey} == ${i}
            RETURN NEW
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 100);
        let results = raw.toArray();
        assertEqual(4, results.length);
        for (let doc of results) {
          assertTrue(i === doc.value);
          assertEqual(doc.test, 1);
        }
      }
    },

    testInnerOuterSameIndex23 : function () {
      dropIndexes(collectionByKey);
      collectionByKey.ensureHashIndex(shardKey);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey}
            UPDATE doc1 WITH {test:1} IN ${cnKey}
            FILTER NEW.${shardKey} == ${i}
            RETURN NEW
        `;

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 100);
        let results = raw.toArray();
        assertEqual(4, results.length);
        for (let doc of results) {
          assertTrue(i === doc.value);
          assertEqual(doc.test, 1);
        }
      }
    },

    testInnerOuterDifferent1 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
        }
      }
    },

    testInnerOuterDifferentIndex1 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
        }
      }
    },

    testInnerOuterDifferent2 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i+1}
              RETURN [doc1, doc2]
        `;
        // Multi-shard not implemented yet
        // validatePlan(query, "EnumerateCollectionNode", collectionByKey1);
        // validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i + 1 === doc2.value);
        }
      }
    },

    testInnerOuterDifferentIndex2 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i+1}
              RETURN [doc1, doc2]
        `;
        // Multi-shard not implemented yet
        // validatePlan(query, "EnumerateCollectionNode", collectionByKey1);
        // validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i + 1 === doc2.value);
        }
      }
    },

    testInnerOuterDifferent3 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferentIndex3 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferent4 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              RETURN doc1
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let doc of results) {
          assertTrue(i === doc.value);
        }
      }
    },

    testInnerOuterDifferentIndex4 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              RETURN doc1
        `;
        validatePlan(query, "IndexNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let doc of results) {
          assertTrue(i === doc.value);
        }
      }
    },

    testInnerOuterDifferent5 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferentIndex5 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleShard);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferent6 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferentIndex6 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FILTER doc1.${shardKey1} == ${i}
            FOR doc2 IN ${cnKey2}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferent7 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc1.${shardKey1} == ${i}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
        }
      }
    },

    testInnerOuterDifferentIndex7 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc1.${shardKey1} == ${i}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
        }
      }
    },

    testInnerOuterDifferent8 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc1.${shardKey1} == ${i}
              FILTER doc2.${shardKey2} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferentIndex8 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc1.${shardKey1} == ${i}
              FILTER doc2.${shardKey2} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferent9 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc1.${shardKey1} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
        }
      }
    },

    testInnerOuterDifferentIndex9 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc1.${shardKey1} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
        }
      }
    },

    testInnerOuterDifferent10 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc1.${shardKey1} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferentIndex10 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc1.${shardKey1} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleShard);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferent11 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            FILTER doc1.${shardKey1} == ${i}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferentIndex11 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            FILTER doc1.${shardKey1} == ${i}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferent12 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              FILTER doc1.${shardKey1} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
        }
      }
    },

    testInnerOuterDifferentIndex12 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              FILTER doc1.${shardKey1} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
        }
      }
    },

    testInnerOuterDifferent13 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              FILTER doc1.${shardKey1} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey1);
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferentIndex13 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              FILTER doc1.${shardKey1} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey1);
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 16);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferent14 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc2[shardKey2]);
        }
      }
    },

    testInnerOuterDifferentIndex14 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              RETURN [doc1, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc2[shardKey2]);
        }
      }
    },

    testInnerOuterDifferent15 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              FILTER NEW.${shardKey1} == ${i}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleShard);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferentIndex15 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              FILTER NEW.${shardKey1} == ${i}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleShard);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(16, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc1[shardKey1]);
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferent16 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferentIndex16 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          FOR doc1 IN ${cnKey1}
            FOR doc2 IN ${cnKey2}
              FILTER doc2.${shardKey2} == ${i}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
              RETURN [NEW, doc2]
        `;
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 400);
        let results = raw.toArray();
        assertEqual(400, results.length);
        for (let [doc1, doc2] of results) {
          assertTrue(i === doc2[shardKey2]);
          assertEqual(doc1.test, 1);
        }
      }
    },

    testInnerOuterDifferent17 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      const i = 17;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            FILTER NEW.${shardKey1} == ${i}
            FILTER doc2.${shardKey2} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleShard);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(16, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey1]);
        assertTrue(i === doc2[shardKey2]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterDifferentIndex17 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      const i = 17;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            FILTER NEW.${shardKey1} == ${i}
            FILTER doc2.${shardKey2} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleShard);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(16, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey1]);
        assertTrue(i === doc2[shardKey2]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterDifferent18 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      const i = 18;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            FILTER NEW.${shardKey1} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(400, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey1]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterDifferentIndex18 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      const i = 18;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            FILTER NEW.${shardKey1} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(400, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey1]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterDifferent19 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      const i = 19;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            FILTER doc2.${shardKey2} == ${i}
            FILTER NEW.${shardKey1} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(16, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey1]);
        assertTrue(i === doc2[shardKey2]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterDifferentIndex19 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      const i = 19;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            FILTER doc2.${shardKey2} == ${i}
            FILTER NEW.${shardKey1} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(16, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc1[shardKey1]);
        assertTrue(i === doc2[shardKey2]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterDifferent20 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      const i = 20;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            FILTER doc2.${shardKey2} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(400, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc2[shardKey2]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterDifferentIndex20 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      const i = 20;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            FILTER doc2.${shardKey2} == ${i}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(400, results.length);
      for (let [doc1, doc2] of results) {
        assertTrue(i === doc2[shardKey2]);
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterDifferent21 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      const i = 21;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(10000, results.length);
      for (let [doc1, doc2] of results) {
        assertEqual(doc1.test, 1);
      }
    },

    testInnerOuterDifferentIndex21 : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      const i = 21;
      const query = `
        FOR doc1 IN ${cnKey1}
          FOR doc2 IN ${cnKey2}
            UPDATE doc1 WITH {test:1} IN ${cnKey1}
            RETURN [NEW, doc2]
      `;

      let raw = db._query(query, {}, disableSingleDocOp);
      let stats = raw.getExtra().stats;
      assertEqual(stats.writesExecuted, 10000);
      let results = raw.toArray();
      assertEqual(10000, results.length);
      for (let [doc1, doc2] of results) {
        assertEqual(doc1.test, 1);
      }
    },

    testSeparateLoops : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          LET ok = (
            FOR doc1 IN ${cnKey1}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
          )
          FOR doc2 IN ${cnKey2}
            FILTER doc2.${shardKey2} == ${i}
            RETURN doc2
        `;
        validatePlan(query, "EnumerateCollectionNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 100);
        let results = raw.toArray();
        assertEqual(4, results.length);
        for (let doc of results) {
          assertEqual(i, doc[shardKey2]);
        }
      }
    },

    testSeparateLoopsIndex : function () {
      dropIndexes(collectionByKey1);
      dropIndexes(collectionByKey2);
      collectionByKey1.ensureHashIndex(shardKey1);
      collectionByKey2.ensureHashIndex(shardKey2);

      for (let i = 0; i < 24; ++i) {
        const query = `
          LET ok = (
            FOR doc1 IN ${cnKey1}
              UPDATE doc1 WITH {test:1} IN ${cnKey1}
          )
          FOR doc2 IN ${cnKey2}
            FILTER doc2.${shardKey2} == ${i}
            RETURN doc2
        `;
        validatePlan(query, "IndexNode", collectionByKey2);

        let raw = db._query(query, {}, disableSingleDocOp);
        let stats = raw.getExtra().stats;
        assertEqual(stats.writesExecuted, 100);
        let results = raw.toArray();
        assertEqual(4, results.length);
        for (let doc of results) {
          assertEqual(i, doc[shardKey2]);
        }
      }
    },

  };
};


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlShardIdsTestSuite);
jsunity.run(ahuacatlShardIdsOptimizationTestSuite);

return jsunity.done();
