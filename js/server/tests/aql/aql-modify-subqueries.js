/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull, assertMatch, fail, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, bind parameters
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

var internal = require("internal");
var db = require("@arangodb").db;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getModifyQueryResultsRaw = helper.getModifyQueryResultsRaw;
var assertQueryError = helper.assertQueryError;
const isCluster = require('@arangodb/cluster').isCluster();
const disableRestrictToSingleShard = { optimizer : { rules : ["-restrict-to-single-shard"] } };
var sanitizeStats = function (stats) {
  // remove these members from the stats because they don't matter
  // for the comparisons
  delete stats.scannedFull;
  delete stats.scannedIndex;
  delete stats.filtered;
  delete stats.executionTime;
  delete stats.httpRequests;
  return stats;
};

let hasDistributeNode = function(nodes) {
  return (nodes.filter(function(node) { 
    return node.type === 'DistributeNode'; 
  }).length > 0);
};

let allNodesOfTypeAreRestrictedToShard = function(nodes, typeName, collection) {
  return nodes.filter(function(node) {
    return node.type === typeName;
  }).every(function(node) {
    return (collection.shards().indexOf(node.restrictedTo) !== -1);
  });
};

let allRemoteNodesAreRestrictedToShard = function(nodes, collection) {
  return nodes.filter(function(node) { 
    return node.type === 'RemoteNode'; 
  }).every(function(node) {
    return (collection.shards().indexOf(node.ownName) !== -1);
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlModifySuite () {
  var errors = internal.errors;
  var cn = "UnitTestsAhuacatlModify";

  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    // use default shard key (_key)
    
    testUpdateSingle : function () {
      if (!isCluster) {
        return;
      }
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      { // no - RestrictToSingleShard
        let key = c.insert({ id: "test", value: 1 })._key;

        let expected = { writesExecuted: 1, writesIgnored: 0 };
        let query = "UPDATE { _key: " + JSON.stringify(key) + ", id: 'test' } WITH { value: 2 } IN " + cn;
        let actual = getModifyQueryResultsRaw(query, {}, disableRestrictToSingleShard);

        let plan = AQL_EXPLAIN(query, {}, disableRestrictToSingleShard).plan;
        //assertTrue(hasDistributeNode(plan.nodes)); // the distribute node is not required here
        assertFalse(allNodesOfTypeAreRestrictedToShard(plan.nodes, 'UpdateNode', c));
        assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));

        assertEqual(1, c.count());
        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
        c.truncate();
      }

      // RestrictToSingleShard
      let key = c.insert({ id: "test", value: 1 })._key;

      let expected = { writesExecuted: 1, writesIgnored: 0 };
      let query = "UPDATE { _key: " + JSON.stringify(key) + ", id: 'test' } WITH { value: 2 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
   
      let plan = AQL_EXPLAIN(query).plan;
      assertFalse(hasDistributeNode(plan.nodes));
      assertTrue(allRemoteNodesAreRestrictedToShard(plan.nodes, c));
      assertNotEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));

      assertEqual(1, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateSingleShardKeyChange : function () {
      if (!isCluster) {
        return;
      }
      
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});
      let key = c.insert({ id: "test", value: 1 })._key;

      assertQueryError(errors.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, "UPDATE { _key: " + JSON.stringify(key) + ", id: 'test' } WITH { value: 2, id: 'bark' } IN " + cn);
    },
    
    testReplaceSingle : function () {
      if (!isCluster) {
        return;
      }
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});
      { // no - RestrictToSingleShard
        let key = c.insert({ id: "test", value: 1 })._key;

        let expected = { writesExecuted: 1, writesIgnored: 0 };
        let query = "REPLACE { _key: " + JSON.stringify(key) + ", id: 'test' } WITH { id: 'test', value: 2 } IN " + cn;
        let actual = getModifyQueryResultsRaw(query, {}, disableRestrictToSingleShard);

        let plan = AQL_EXPLAIN(query, {}, disableRestrictToSingleShard).plan;
        assertTrue(hasDistributeNode(plan.nodes));
        assertFalse(allNodesOfTypeAreRestrictedToShard(plan.nodes, 'ReplaceNode', c));
        assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
        c.truncate();
      }

      // RestrictToSingleShard
      let key = c.insert({ id: "test", value: 1 })._key;

      let expected = { writesExecuted: 1, writesIgnored: 0 };
      let query = "REPLACE { _key: " + JSON.stringify(key) + ", id: 'test' } WITH { id: 'test', value: 2 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
   
      let plan = AQL_EXPLAIN(query).plan;
      assertFalse(hasDistributeNode(plan.nodes));
      assertTrue(allRemoteNodesAreRestrictedToShard(plan.nodes, c));
      assertNotEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));

      assertEqual(1, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceSingleShardKeyChange : function () {
      if (!isCluster) {
        return;
      }
      
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});
      let key = c.insert({ id: "test", value: 1 })._key;

      assertQueryError(errors.ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES.code, "REPLACE { _key: " + JSON.stringify(key) + ", id: 'test' } WITH { value: 2, id: 'bark' } IN " + cn);
    },
    
    testInsertMainLevelWithCustomShardKeyConstant : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      // no - RestrictToSingleShard
      for (let i = 0; i < 30; ++i) {
        let expected = { writesExecuted: 1, writesIgnored: 0 };
        let query = "INSERT { value: " + i + ", id: 'test" + i + "' } IN " + cn;
        let actual = getModifyQueryResultsRaw(query, {}, disableRestrictToSingleShard);

        if (isCluster) {
          let plan = AQL_EXPLAIN(query, {}, disableRestrictToSingleShard).plan;
          assertTrue(hasDistributeNode(plan.nodes));
          assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
        }

        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
      }
      assertEqual(30, c.count());
      c.truncate();

      // RestrictToSingleShard
      for (let i = 0; i < 30; ++i) {
        let expected = { writesExecuted: 1, writesIgnored: 0 };
        let query = "INSERT { value: " + i + ", id: 'test" + i + "' } IN " + cn;
        let actual = getModifyQueryResultsRaw(query);
        
        if (isCluster) {
          let plan = AQL_EXPLAIN(query).plan;
          assertFalse(hasDistributeNode(plan.nodes));
          assertNotEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
        }
      
        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
      } 
      assertEqual(30, c.count());
      
      for (let i = 0; i < 30; ++i) {
        let r = db._query("FOR doc IN " + cn + " FILTER doc.id == 'test" + i + "' RETURN doc").toArray();
        assertEqual(1, r.length);
        assertEqual("test" + i, r[0].id);
      }
    },
    
    testInsertMainLevelWithCustomShardKeyMultiLevel : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["a.b"]});


      for (let i = 0; i < 30; ++i) {
        let expected = { writesExecuted: 1, writesIgnored: 0 };
        let query = "INSERT { value: " + i + ", a: { b: 'test" + i + "' } } IN " + cn;
        let actual = getModifyQueryResultsRaw(query);
        
        if (isCluster) {
          let plan = AQL_EXPLAIN(query).plan;
          assertTrue(hasDistributeNode(plan.nodes));
          assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
        }
      
        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
      } 
      assertEqual(30, c.count());
      
      for (let i = 0; i < 30; ++i) {
        let r = db._query("FOR doc IN " + cn + " FILTER doc.a.b == 'test" + i + "' RETURN doc").toArray();
        assertEqual(1, r.length);
        assertEqual("test" + i, r[0].a.b);
      }
    },
    
    testInsertMainLevelWithKeyConstant : function () {
      let c = db._create(cn, {numberOfShards:5});

      // no - RestrictToSingleShard
      for (let i = 0; i < 30; ++i) {
        let expected = { writesExecuted: 1, writesIgnored: 0 };
        let query = "INSERT { value: " + i + ", a: { b: 'test" + i + "' } } IN " + cn;
        let actual = getModifyQueryResultsRaw(query);

        if (isCluster) {
          let plan = AQL_EXPLAIN(query).plan;
          assertTrue(hasDistributeNode(plan.nodes));
          assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
        }

        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
      }
      assertEqual(30, c.count());
      c.truncate();

      // RestrictToSingleShard
      for (let i = 0; i < 30; ++i) {
        let expected = { writesExecuted: 1, writesIgnored: 0 };
        let query = "INSERT { value: " + i + ", _key: 'test" + i + "' } IN " + cn;
        let actual = getModifyQueryResultsRaw(query);
        
        if (isCluster) {
          let plan = AQL_EXPLAIN(query).plan;
          assertFalse(hasDistributeNode(plan.nodes));
          assertNotEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
        }
      
        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
      } 
      assertEqual(30, c.count());
      
      for (let i = 0; i < 30; ++i) {
        let r = db._query("FOR doc IN " + cn + " FILTER doc._key == 'test" + i + "' RETURN doc").toArray();
        assertEqual(1, r.length);
        assertEqual("test" + i, r[0]._key);
        assertEqual(cn + "/test" + i, r[0]._id);
        
        r = db._query("FOR doc IN " + cn + " FILTER doc._id == '" + cn + "/test" + i + "' RETURN doc").toArray();
        assertEqual(1, r.length);
        assertEqual("test" + i, r[0]._key);
        assertEqual(cn + "/test" + i, r[0]._id);
      }
    },
  
    testInsertMainLevelWithKeyExpression : function () {
      let c = db._create(cn, {numberOfShards:5});
      // no - RestrictToSingleShard
      for (let i = 0; i < 30; ++i) {
        let expected = { writesExecuted: 1, writesIgnored: 0 };
        let query = "INSERT { value: " + i + ", _key: 'test" + i + "' } IN " + cn;
        let actual = getModifyQueryResultsRaw(query, {}, disableRestrictToSingleShard);

        if (isCluster) {
          let plan = AQL_EXPLAIN(query, {}, disableRestrictToSingleShard).plan;
          assertTrue(hasDistributeNode(plan.nodes));
          assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
        }

        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
      }
      assertEqual(30, c.count());

      for (let i = 0; i < 30; ++i) {
        let r = db._query("FOR doc IN " + cn + " FILTER doc._key == 'test" + i + "' RETURN doc").toArray();
        assertEqual(1, r.length);
        assertEqual("test" + i, r[0]._key);
        assertEqual(cn + "/test" + i, r[0]._id);

        r = db._query("FOR doc IN " + cn + " FILTER doc._id == '" + cn + "/test" + i + "' RETURN doc").toArray();
        assertEqual(1, r.length);
        assertEqual("test" + i, r[0]._key);
        assertEqual(cn + "/test" + i, r[0]._id);
      }
      c.truncate();

      // RestrictToSingleShard
      for (let i = 0; i < 30; ++i) {
        let expected = { writesExecuted: 1, writesIgnored: 0 };
        let query = "INSERT { value: " + i + ", _key: NOOPT(CONCAT('test', '" + i + "')) } IN " + cn;
        let actual = getModifyQueryResultsRaw(query);
        
        if (isCluster) {
          let plan = AQL_EXPLAIN(query).plan;
          assertTrue(hasDistributeNode(plan.nodes));
          assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
        }
      
        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
      } 
      assertEqual(30, c.count());
      
      for (let i = 0; i < 30; ++i) {
        let r = db._query("FOR doc IN " + cn + " FILTER doc._key == 'test" + i + "' RETURN doc").toArray();
        assertEqual(1, r.length);
        assertEqual("test" + i, r[0]._key);
        assertEqual(cn + "/test" + i, r[0]._id);
        
        r = db._query("FOR doc IN " + cn + " FILTER doc._id == '" + cn + "/test" + i + "' RETURN doc").toArray();
        assertEqual(1, r.length);
        assertEqual("test" + i, r[0]._key);
        assertEqual(cn + "/test" + i, r[0]._id);
      }
    },
    
    testInsertMainLevelWithKey : function () {
      let c = db._create(cn, {numberOfShards:5});

      let expected = { writesExecuted: 2000, writesIgnored: 0 };
      let query = "FOR i IN 1..2000 INSERT { value: i, _key: CONCAT('test', i) } IN " + cn;
      { // no - RestrictToSingleShard
        let actual = getModifyQueryResultsRaw(query);

        if (isCluster) {
          let nodes = AQL_EXPLAIN(query, {}, disableRestrictToSingleShard).plan.nodes;
          assertTrue(hasDistributeNode(nodes));
        }

        assertEqual(2000, c.count());
        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
        c.truncate();
      }
      // RestrictToSingleShard
      let actual = getModifyQueryResultsRaw(query);
      
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(2000, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testInsertMainLevelWithKeyReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5});

      let expected = { writesExecuted: 2000, writesIgnored: 0 };
      let query = "FOR i IN 1..2000 INSERT { value: i, _key: CONCAT('test', i) } IN " + cn +  " RETURN NEW";
      let actual = getModifyQueryResultsRaw(query);
      
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(2000, c.count());
      assertEqual(2000, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testInsertMainLevelCustomShardKey : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      let expected = { writesExecuted: 2000, writesIgnored: 0 };
      let query = "FOR i IN 1..2000 INSERT { value: i, id: i } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(2000, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testInsertMainLevelCustomShardKeyReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      let expected = { writesExecuted: 2000, writesIgnored: 0 };
      let query = "FOR i IN 1..2000 INSERT { value: i, id: i } IN " + cn + " RETURN NEW";
      let actual = getModifyQueryResultsRaw(query);
      
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(2000, c.count());
      assertEqual(2000, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testInsertMainLevel : function () {
      let c = db._create(cn, {numberOfShards:5});

      let expected = { writesExecuted: 2000, writesIgnored: 0 };
      let query = "FOR i IN 1..2000 INSERT { value: i } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(2000, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testInsertMainLevelWithReturn : function () {
      let c = db._create(cn, {numberOfShards:5});

      let expected = { writesExecuted: 2000, writesIgnored: 0 };
      let query = "FOR i IN 1..2000 INSERT { value: i } IN " + cn + " RETURN NEW";
      let actual = getModifyQueryResultsRaw(query);
      
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(2000, c.count());
      assertEqual(2000, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testInsertInSubquery : function () {
      let c = db._create(cn, {numberOfShards:5});

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR i IN 1..100 INSERT { value: i } IN " + cn + ")");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual([ [ ] ], actual.json);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testInsertInSubqueryWithReturn : function () {
      let c = db._create(cn, {numberOfShards:5});

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR i IN 1..100 INSERT { value: i } IN " + cn + " RETURN NEW)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelWithKeySingle : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 1, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " FILTER d._key == 'test93' REMOVE d IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertTrue(allRemoteNodesAreRestrictedToShard(plan.nodes, c));
        assertNotEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
      }
    
      assertEqual(99, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelWithKeySingleReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 1, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " FILTER d._key == 'test93' REMOVE d IN " + cn + " RETURN OLD";
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertTrue(allRemoteNodesAreRestrictedToShard(plan.nodes, c));
        assertNotEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
      }
    
      assertEqual(99, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelWithoutKey : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let query = "FOR d IN " + cn + " REMOVE { foo: 'bar' } IN " + cn;
      
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_KEY_MISSING.code, query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }
    
      assertEqual(100, c.count());
    },
    
    testRemoveMainLevelSingleKey : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let query = "FOR d IN " + cn + " REMOVE { _key: 'bar' } IN " + cn;
     
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, query);

      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }
    
      assertEqual(100, c.count());
    },
    
    testRemoveMainLevelWithKey : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertNotEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }
    
      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelWithKeyReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key } IN " + cn + " RETURN OLD";
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertNotEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }
    
      assertEqual(0, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelWithKey2 : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR i IN 0..99 REMOVE { _key: CONCAT('test', i) } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelWithKey2ReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR i IN 0..99 REMOVE { _key: CONCAT('test', i) } IN " + cn + " RETURN OLD";
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(0, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelDefaultShardKeyByReference : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE d IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertNotEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }

      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelDefaultShardKeyByAttribute : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE d._key IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertNotEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }

      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelDefaultShardKeyByObject : function () {
      let c = db._create(cn, {numberOfShards:5});

      { // no - RestrictToSingleShard
        for (let i = 0; i < 100; ++i) {
          c.insert({ id: i });
        }

        let expected = { writesExecuted: 100, writesIgnored: 0 };
        let query = "FOR d IN " + cn + " REMOVE { _key: d._key } IN " + cn;
        let actual = getModifyQueryResultsRaw(query, {}, disableRestrictToSingleShard);
        if (isCluster) {
          let plan = AQL_EXPLAIN(query, {}, disableRestrictToSingleShard).plan;
          assertFalse(hasDistributeNode(plan.nodes));
          assertNotEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
        }

        assertEqual(0, c.count());
        assertEqual(0, actual.json.length);
        assertEqual(expected, sanitizeStats(actual.stats));
      }
      //  RestrictToSingleShard
      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertNotEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }

      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeyFixed : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: 42 });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key, id: 42 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }

      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeyFixedSingle : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      // this will still go to all the shards!
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key, id: 42 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
        assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
      }

      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeyFixedSingleWithFilter : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 1, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " FILTER d.id == 42 REMOVE { _key: d._key, id: 42 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
        assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
      }

      assertEqual(99, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeyWithIdFixedSingleWithFilter : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});
      c.ensureIndex({ type: "hash", fields: ["id"] });

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 1, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " FILTER d.id == 42 REMOVE { _key: d._key, id: 42 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
        assertNotEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
      }

      assertEqual(99, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeys : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id1", "id2"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id1: i, id2: i % 10 });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key, id1: d.id1, id2: d.id2 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertNotEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }

      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeysFixed : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id1", "id2"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id1: i, id2: i % 10 });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      // this will delete all documents!
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key, id1: d.id1, id2: 2 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
        assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
      }

      // this will actually delete all the documents
      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeysFixedWithFilter : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id1", "id2"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id1: i, id2: i % 10 });
      }

      let expected = { writesExecuted: 10, writesIgnored: isCluster ? (internal.isEnterprise() ? 40 : 36) : 0 };
      let query = "FOR d IN " + cn + " FILTER d.id2 == 2 REMOVE { _key: d._key, id1: d.id1, id2: 2 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }

      assertEqual(90, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeysWithFilterIndexed : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id1", "id2"]});
      c.ensureIndex({ type: "hash", fields: ["id1", "id2"] });

      for (let i = 0; i < 100; ++i) {
        c.insert({ id1: i, id2: i % 10 });
      }

      let expected = { writesExecuted: 3, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " FILTER d.id1 IN [ 2, 12, 22 ] && d.id2 == 2 REMOVE { _key: d._key, id1: d.id1, id2: d.id2 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertNotEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }

      assertEqual(97, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeysMissing : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id1", "id2"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id1: i, id2: i % 10 });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key, id1: d.id1 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
        assertEqual(-1, plan.rules.indexOf("restrict-to-single-shard"));
      }

      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKey : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key, id: d.id } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let plan = AQL_EXPLAIN(query).plan;
        assertFalse(hasDistributeNode(plan.nodes));
        assertNotEqual(-1, plan.rules.indexOf("undistribute-remove-after-enum-coll"));
      }

      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKey2 : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE d IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertFalse(hasDistributeNode(nodes));
      }
    
      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelCustomShardKeyReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE { _key: d._key, id: d.id } IN " + cn + " RETURN OLD";
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertFalse(hasDistributeNode(nodes));
      }
    
      assertEqual(0, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevel : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE d IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertFalse(hasDistributeNode(nodes));
      }
    
      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveMainLevelWithReturn : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " REMOVE d IN " + cn + " RETURN OLD";
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertFalse(hasDistributeNode(nodes));
      }
    
      assertEqual(0, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveInSubquery : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REMOVE d IN " + cn + ")");
    
      assertEqual(0, c.count());
      assertEqual(1, actual.json.length);
      assertEqual([ [ ] ], actual.json);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveInSubqueryWithReturn : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REMOVE d IN " + cn + " RETURN OLD)");
    
      assertEqual(0, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateMainLevel : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn;
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(100, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateMainLevelWithReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let query = "FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + " RETURN OLD";
      let actual = getModifyQueryResultsRaw(query);
      if (isCluster) {
        let nodes = AQL_EXPLAIN(query).plan.nodes;
        assertTrue(hasDistributeNode(nodes));
      }
    
      assertEqual(100, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateMainLevelWithReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + " RETURN NEW");
    
      assertEqual(100, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateInSubquery : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + ")");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual([ [ ] ], actual.json);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateInSubqueryWithReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + " RETURN OLD)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateInSubqueryWithReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + " RETURN NEW)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },

    testReplaceMainLevel : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " REPLACE d WITH { value: 2 } IN " + cn);
    
      assertEqual(100, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceMainLevelWithReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " REPLACE d WITH { value: 2 } IN " + cn + " RETURN OLD");
    
      assertEqual(100, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceMainLevelWithReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " REPLACE d WITH { value: 2 } IN " + cn + " RETURN NEW");
    
      assertEqual(100, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceInSubquery : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REPLACE d WITH { value: 2 } IN " + cn + ")");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual([ [ ] ], actual.json);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceInSubqueryWithReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REPLACE d WITH { value: 2 } IN " + cn + " RETURN OLD)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceInSubqueryWithReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5});

      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REPLACE d WITH { value: 2 } IN " + cn + " RETURN NEW)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
   
    // use custom shard key 
    
    testInsertMainLevelCustomShardKeyWithReturn : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR i IN 1..100 INSERT { id: i, value: i } IN " + cn + " RETURN NEW");
    
      assertEqual(100, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testInsertCustomShardKeyInSubquery : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR i IN 1..100 INSERT { id: i, value: i } IN " + cn + ")");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual([ [ ] ], actual.json);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testInsertCustomShardKeyInSubqueryWithReturn : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR i IN 1..100 INSERT { id: i, value: i } IN " + cn + " RETURN NEW)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveCustomShardKeyMainLevel : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " REMOVE d IN " + cn);
    
      assertEqual(0, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveCustomShardKeyMainLevelWithReturn : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " REMOVE d IN " + cn + " RETURN OLD");
    
      assertEqual(0, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testRemoveCustomShardKeyInSubquery : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      { // no - RestrictToSingleShard
        for (let i = 0; i < 100; ++i) {
          c.insert({ id: i });
        }

        let expected = { writesExecuted: 100, writesIgnored: 0 };
        let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REMOVE d IN " + cn + ")", {}, disableRestrictToSingleShard);

        assertEqual(0, c.count());
        assertEqual(1, actual.json.length);
        assertEqual([ [ ] ], actual.json);
        assertEqual(expected, sanitizeStats(actual.stats));
      }

      { // RestrictToSingleShard
        for (let i = 0; i < 100; ++i) {
          c.insert({ id: i });
        }

        let expected = { writesExecuted: 100, writesIgnored: 0 };
        let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REMOVE d IN " + cn + ")");
      
        assertEqual(0, c.count());
        assertEqual(1, actual.json.length);
        assertEqual([ [ ] ], actual.json);
        assertEqual(expected, sanitizeStats(actual.stats));
      }
    },
    
    testRemoveCustomShardKeyInSubqueryWithReturn : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REMOVE d IN " + cn + " RETURN OLD)");
    
      assertEqual(0, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateCustomShardKeyMainLevel : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn);
    
      assertEqual(100, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateCustomShardKeyMainLevelWithReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + " RETURN OLD");
    
      assertEqual(100, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateCustomShardKeyMainLevelWithReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + " RETURN NEW");
    
      assertEqual(100, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateCustomShardKeyInSubquery : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + ")");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual([ [ ] ], actual.json);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateCustomShardKeyInSubqueryWithReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + " RETURN OLD)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testUpdateCustomShardKeyInSubqueryWithReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: isCluster ? 400 : 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " UPDATE d WITH { value: 2 } IN " + cn + " RETURN NEW)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceCustomShardKeyMainLevel : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " REPLACE d WITH { id: d.id, value: 2 } IN " + cn);
    
      assertEqual(100, c.count());
      assertEqual(0, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceCustomShardKeyMainLevelWithReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " REPLACE d WITH { id: d.id, value: 2 } IN " + cn + " RETURN OLD");
    
      assertEqual(100, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceCustomShardKeyMainLevelWithReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("FOR d IN " + cn + " REPLACE d WITH { id: d.id, value: 2 } IN " + cn + " RETURN NEW");
    
      assertEqual(100, c.count());
      assertEqual(100, actual.json.length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceCustomShardKeyInSubquery : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REPLACE d WITH { id: d.id, value: 2 } IN " + cn + ")");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual([ [ ] ], actual.json);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceCustomShardKeyInSubqueryWithReturnOld : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REPLACE d WITH { id: d.id, value: 2 } IN " + cn + " RETURN OLD)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },
    
    testReplaceCustomShardKeyInSubqueryWithReturnNew : function () {
      let c = db._create(cn, {numberOfShards:5, shardKeys: ["id"]});

      for (let i = 0; i < 100; ++i) {
        c.insert({ id: i });
      }

      let expected = { writesExecuted: 100, writesIgnored: 0 };
      let actual = getModifyQueryResultsRaw("RETURN (FOR d IN " + cn + " REPLACE d WITH { id: d.id, value: 2 } IN " + cn + " RETURN NEW)");
    
      assertEqual(100, c.count());
      assertEqual(1, actual.json.length);
      assertEqual(100, actual.json[0].length);
      assertEqual(expected, sanitizeStats(actual.stats));
    },

  };
}

jsunity.run(ahuacatlModifySuite);

return jsunity.done();
