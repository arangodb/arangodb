/*jshint globalstrict:false, strict:false */
/* global arango, getOptions, runSetup, assertTrue, assertFalse, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'rocksdb.auto-refill-index-caches-on-modify' : 'true',
    'server.statistics' : 'false',
    'foxx.queues' : 'false',
  };
}

const cn = 'UnitTestsCollection';
const db = require('@arangodb').db;
const jsunity = require('jsunity');
const getMetric = require('@arangodb/test-helper').getMetricSingle;
const time = require('internal').time;

function AutoRefillIndexCachesEdge() {
  'use strict';

  let runCheck = () => {
    let crsr = db._query(`FOR i IN 0..9999 FOR doc IN ${cn} FILTER doc._from == CONCAT('v/test', i) RETURN doc._from`);
    let res = crsr.toArray();
    assertEqual(10000, res.length);
    res.forEach((val, i) => {
      assertEqual(val, 'v/test' + i);
    });
    let stats = crsr.getExtra().stats;
    assertTrue(stats.cacheHits > 0, stats);

    crsr = db._query(`FOR i IN 0..24 FOR doc IN ${cn} FILTER doc._to == CONCAT('v/test', i) RETURN doc._to`);
    res = crsr.toArray();
    assertEqual(10000, res.length);
    res.forEach((val, i) => {
      assertEqual(val, 'v/test' + Math.floor(i / 400));
    });
    stats = crsr.getExtra().stats;
    assertTrue(stats.cacheHits > 0, stats);
  };

  return {
    setUp: function() {
      db._createEdgeCollection(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },
    
    testInsert: function() {
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR i IN 0..9999 INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * 10000, { oldValue, newValue });

      require('internal').sleep(3);
      runCheck();
    },
    
    testUpdate: function() {
      db._query(`FOR i IN 0..9999 INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} UPDATE doc WITH {value: doc.value + 1} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * 10000, { oldValue, newValue });

      require('internal').sleep(3);
      runCheck();
    },
    
    testReplace: function() {
      db._query(`FOR i IN 0..9999 INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} REPLACE doc WITH {_from: doc._from, _to: doc._to, value: doc.value + 1} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * 10000, { oldValue, newValue });

      require('internal').sleep(3);
      runCheck();
    },
    
  };
}

function AutoRefillIndexCachesVPack() {
  'use strict';

  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },
    
    testInsertNoIndex: function() {
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR i IN 0..9999 INSERT {} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
    },
    
    testInsertWithIndexNoCache: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["value"] });
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });

      let crsr = db._query(`FOR i IN 0..9999 FOR doc IN ${cn} FILTER doc.value == i RETURN doc.value`);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, i);
      });
      let stats = crsr.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
    },
    
    testInsertWithIndexWithCache: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 10000, { oldValue, newValue });

      require('internal').sleep(3);
      
      let crsr = db._query(`FOR i IN 0..9999 FOR doc IN ${cn} FILTER doc.value == i RETURN doc.value`);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, i);
      });
      let stats = crsr.getExtra().stats;
      assertTrue(stats.cacheHits > 0, stats);
    },
    
    testUpdateWithIndexNoCache: function() {
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      db[cn].ensureIndex({ type: "persistent", fields: ["value"] });
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} UPDATE doc WITH {value: doc.value + 1} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });

      let crsr = db._query(`FOR i IN 1..10000 FOR doc IN ${cn} FILTER doc.value == i RETURN doc.value`);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, i + 1);
      });
      let stats = crsr.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
    },
    
    testUpdateWithIndexWithCache: function() {
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      db[cn].ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} UPDATE doc WITH {value: doc.value + 1} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 10000, { oldValue, newValue });

      require('internal').sleep(3);
      
      let crsr = db._query(`FOR i IN 1..10000 FOR doc IN ${cn} FILTER doc.value == i RETURN doc.value`);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, i + 1);
      });
      let stats = crsr.getExtra().stats;
      assertTrue(stats.cacheHits > 0, stats);
    },
    
    testReplaceWithIndexNoCache: function() {
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      db[cn].ensureIndex({ type: "persistent", fields: ["value"] });
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} REPLACE doc WITH {value: doc.value + 1} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });

      require('internal').sleep(3);
      
      let crsr = db._query(`FOR i IN 1..10000 FOR doc IN ${cn} FILTER doc.value == i RETURN doc.value`);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, i + 1);
      });
      let stats = crsr.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
    },
    
    testReplaceWithIndexWithCache: function() {
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      db[cn].ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} REPLACE doc WITH {value: doc.value + 1} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 10000, { oldValue, newValue });

      require('internal').sleep(3);
      
      let crsr = db._query(`FOR i IN 1..10000 FOR doc IN ${cn} FILTER doc.value == i RETURN doc.value`);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, i + 1);
      });
      let stats = crsr.getExtra().stats;
      assertTrue(stats.cacheHits > 0, stats);
    },

  };
}

jsunity.run(AutoRefillIndexCachesEdge);
jsunity.run(AutoRefillIndexCachesVPack);
return jsunity.done();
