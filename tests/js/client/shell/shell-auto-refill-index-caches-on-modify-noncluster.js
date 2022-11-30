/*jshint globalstrict:false, strict:false */
/* global arango, getOptions, runSetup, assertTrue, assertFalse, assertEqual */

////////////////////////////////////////////////////////////////////////////////
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

const db = require('@arangodb').db;
const jsunity = require('jsunity');
const getMetric = require('@arangodb/test-helper').getMetricSingle;

const cn = 'UnitTestsCollection';
const n = 5000; // documents

// wait for metrics to settle. 3 seconds is arbitrary, and may not even
// be enough on a busy CI server
const waitForMetrics = () => {
  require('internal').sleep(3);
};

function AutoRefillIndexCachesEdge() {
  'use strict';

  let runCheck = (expectHits) => {
    let crsr = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc._from == CONCAT('v/test', i) RETURN doc._from`);
    let res = crsr.toArray();
    assertEqual(n, res.length);
    res.forEach((val, i) => {
      assertEqual(val, 'v/test' + i);
    });
    let stats = crsr.getExtra().stats;
    if (expectHits) {
      assertTrue(stats.cacheHits > 0, stats);
    } else {
      assertEqual(0, stats.cacheHits, stats);
    }

    crsr = db._query(`FOR i IN 0..24 FOR doc IN ${cn} FILTER doc._to == CONCAT('v/test', i) RETURN doc._to`);
    res = crsr.toArray();
    assertEqual(n, res.length);
    res.forEach((val, i) => {
      assertEqual(val, 'v/test' + Math.floor(i / (n / 25)));
    });
    stats = crsr.getExtra().stats;
    if (expectHits) {
      assertTrue(stats.cacheHits > 0, stats);
    } else {
      assertEqual(0, stats.cacheHits, stats);
    }
  };

  return {
    setUp: function() {
      db._createEdgeCollection(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },
    
    testInsertEdgeAqlDisabled: function() {
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR i IN 0..${n - 1} INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn} OPTIONS { refillIndexCaches: false }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(false);
    },
    
    testInsertEdgeAqlEnabled: function() {
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR i IN 0..${n - 1} INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn} OPTIONS { refillIndexCaches: true }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * n, { oldValue, newValue });
      waitForMetrics();
      runCheck(true);
    },
    
    testInsertEdgeBatchDisabled: function() {
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({_from: 'v/test' + i, _to: 'v/test' + (i % 25)});
      }
      db[cn].insert(docs, { refillIndexCaches: false });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(false);
    },
    
    testInsertEdgeBatchEnabled: function() {
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({_from: 'v/test' + i, _to: 'v/test' + (i % 25)});
      }
      db[cn].insert(docs, { refillIndexCaches: true });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * n, { oldValue, newValue });
      waitForMetrics();
      runCheck(true);
    },
    
    testUpdateEdgeAqlDisabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} UPDATE doc WITH {value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: false }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(false);
    },
    
    testUpdateEdgeAqlEnabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} UPDATE doc WITH {value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: true }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * n, { oldValue, newValue });
      waitForMetrics();
      runCheck(true);
    },
    
    testUpdateEdgeBatchDisabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i), _from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let keys = [];
      let docs = [];
      for (let i = 0; i < n; ++i) {
        keys.push({_key: 'test' + i});
        docs.push({value: i + 1 });
      }
      db[cn].update(keys, docs, { refillIndexCaches: false });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(false);
    },
    
    testUpdateEdgeBatchEnabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i), _from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let keys = [];
      let docs = [];
      for (let i = 0; i < n; ++i) {
        keys.push({_key: 'test' + i});
        docs.push({value: i + 1 });
      }
      db[cn].update(keys, docs, { refillIndexCaches: true });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * n, { oldValue, newValue });
      waitForMetrics();
      runCheck(true);
    },
    
    testReplaceEdgeAqlDisabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} REPLACE doc WITH {_from: doc._from, _to: doc._to, value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: false }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(false);
    },
    
    testReplaceEdgeAqlEnabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} REPLACE doc WITH {_from: doc._from, _to: doc._to, value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: true }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * n, { oldValue, newValue });
      waitForMetrics();
      runCheck(true);
    },
    
    testReplaceEdgeBatchDisabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i), _from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let keys = [];
      let docs = [];
      for (let i = 0; i < n; ++i) {
        keys.push({_key: 'test' + i});
        docs.push({_from: 'v/test' + i, _to: 'v/test' + (i % 25), value: i + 1 });
      }
      db[cn].replace(keys, docs, { refillIndexCaches: false });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(false);
    },
    
    testReplaceEdgeBatchEnabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i), _from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let keys = [];
      let docs = [];
      for (let i = 0; i < n; ++i) {
        keys.push({_key: 'test' + i});
        docs.push({_from: 'v/test' + i, _to: 'v/test' + (i % 25), value: i + 1 });
      }
      db[cn].replace(keys, docs, { refillIndexCaches: true });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * n, { oldValue, newValue });
      waitForMetrics();
      runCheck(true);
    },
  };
}

function AutoRefillIndexCachesVPack() {
  'use strict';
     
  let runCheck = (offset, expectHits) => {
    let crsr = db._query(`FOR i IN ${0 + offset}..${n + offset} FOR doc IN ${cn} FILTER doc.value == i RETURN doc.value`);
    let res = crsr.toArray();
    assertEqual(n, res.length);
    res.forEach((val, i) => {
      assertEqual(val, i + offset);
    });
    let stats = crsr.getExtra().stats;
    if (expectHits) {
      assertTrue(stats.cacheHits > 0, stats);
    } else {
      assertEqual(0, stats.cacheHits, stats);
    }
  };

  return {
    setUp: function() {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
    },

    tearDown: function() {
      db._drop(cn);
    },
    
    testInsertVPackAqlDisabled: function() {
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR i IN 0..${n - 1} INSERT {value: i} INTO ${cn} OPTIONS { refillIndexCaches: false }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(0, false);
    },
    
    testInsertVPackAqlEnabled: function() {
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR i IN 0..${n - 1} INSERT {value: i} INTO ${cn} OPTIONS { refillIndexCaches: true }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= n, { oldValue, newValue });
      waitForMetrics();
      runCheck(0, true);
    },
    
    testUpdateVPackAqlDisabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {value: i} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} UPDATE doc WITH {value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: false }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(1, false);
    },
    
    testUpdateVPackAqlEnabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {value: i} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} UPDATE doc WITH {value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: true }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= n, { oldValue, newValue });
      waitForMetrics();
      runCheck(1, true);
    },
    
    testUpdateVPackBatchDisabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i), value: i} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let keys = [];
      let docs = [];
      for (let i = 0; i < n; ++i) {
        keys.push({_key: 'test' + i});
        docs.push({value: i + 1});
      }
      db[cn].update(keys, docs, { refillIndexCaches: false });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(1, false);
    },
    
    testUpdateVPackBatchEnabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i), value: i} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let keys = [];
      let docs = [];
      for (let i = 0; i < n; ++i) {
        keys.push({_key: 'test' + i});
        docs.push({value: i + 1});
      }
      db[cn].update(keys, docs, { refillIndexCaches: true });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      
      assertTrue(newValue - oldValue >= n, { oldValue, newValue });
      waitForMetrics();
      runCheck(1, true);
    },
    
    testReplaceVPackAqlDisabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {value: i} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} REPLACE doc WITH {value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: false }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      
      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(1, false);
    },
    
    testReplaceVPackAqlEnabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {value: i} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} REPLACE doc WITH {value: doc.value + 1} INTO ${cn} OPTIONS { refillIndexCaches: true }`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      
      assertTrue(newValue - oldValue >= n, { oldValue, newValue });
      waitForMetrics();
      runCheck(1, true);
    },
    
    testReplaceVPackBatchDisabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i), value: i} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let keys = [];
      let docs = [];
      for (let i = 0; i < n; ++i) {
        keys.push({_key: 'test' + i});
        docs.push({value: i + 1});
      }
      db[cn].replace(keys, docs, { refillIndexCaches: false });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      // 10 here because there may be background operations running that
      // could affect the metrics
      assertTrue(newValue - oldValue <= 10, { oldValue, newValue });
      waitForMetrics();
      runCheck(1, false);
    },
    
    testReplaceVPackBatchEnabled: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i), value: i} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      let keys = [];
      let docs = [];
      for (let i = 0; i < n; ++i) {
        keys.push({_key: 'test' + i});
        docs.push({value: i + 1});
      }
      db[cn].replace(keys, docs, { refillIndexCaches: true });
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= n, { oldValue, newValue });
      waitForMetrics();
      runCheck(1, true);
    },

  };
}

jsunity.run(AutoRefillIndexCachesEdge);
jsunity.run(AutoRefillIndexCachesVPack);
return jsunity.done();
