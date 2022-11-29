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
    'rocksdb.auto-refill-index-caches-on-modify' : 'false',
    'server.statistics' : 'false',
    'foxx.queues' : 'false',
  };
}

const cn = 'UnitTestsCollection';
const db = require('@arangodb').db;
const jsunity = require('jsunity');
const getMetric = require('@arangodb/test-helper').getMetricSingle;
const time = require('internal').time;

function AutoRefillIndexCaches() {
  'use strict';

  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },
    
    testInsertDisabled: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
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
    
    testInsertEnabled: function() {
      db[cn].ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn} OPTIONS { refillIndexCaches: true }`);
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
    
    testUpdateDisabled: function() {
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      db[cn].ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
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
    
    testUpdateEnabled: function() {
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      db[cn].ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} UPDATE doc WITH {value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: true }`);
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
    
    testReplaceDisabled: function() {
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      db[cn].ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
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
    
    testReplaceEnabled: function() {
      db._query(`FOR i IN 0..9999 INSERT {value: i} INTO ${cn}`);
      db[cn].ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} REPLACE doc WITH {value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: true }`);
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

jsunity.run(AutoRefillIndexCaches);
return jsunity.done();
