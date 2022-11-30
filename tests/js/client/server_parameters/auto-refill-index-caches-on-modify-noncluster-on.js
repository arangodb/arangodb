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

const db = require('@arangodb').db;
const jsunity = require('jsunity');
const getMetric = require('@arangodb/test-helper').getMetricSingle;
const time = require('internal').time;

const cn = 'UnitTestsCollection';
const n = 5000;

// wait for metrics to settle. 5 seconds is arbitrary, and may not even
// be enough on a busy CI server
const waitForMetrics = () => {
  require('internal').sleep(5);
};

function AutoRefillIndexCachesEdge() {
  'use strict';

  let runCheck = () => {
    let crsr = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc._from == CONCAT('v/test', i) RETURN doc._from`);
    let res = crsr.toArray();
    assertEqual(n, res.length);
    res.forEach((val, i) => {
      assertEqual(val, 'v/test' + i);
    });
    let stats = crsr.getExtra().stats;
    assertTrue(stats.cacheHits > 0, stats);

    crsr = db._query(`FOR i IN 0..24 FOR doc IN ${cn} FILTER doc._to == CONCAT('v/test', i) RETURN doc._to`);
    res = crsr.toArray();
    assertEqual(n, res.length);
    res.forEach((val, i) => {
      assertEqual(val, 'v/test' + Math.floor(i / (n / 25)));
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
      db._query(`FOR i IN 0..${n - 1} INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * n, { oldValue, newValue });
      waitForMetrics();
      runCheck();
    },
    
    testUpdateEdge: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} UPDATE doc WITH {value: doc.value + 1} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * n, { oldValue, newValue });
      waitForMetrics();
      runCheck();
    },
    
    testReplaceEdge: function() {
      db._query(`FOR i IN 0..${n - 1} INSERT {_from: CONCAT('v/test', i), _to: CONCAT('v/test', (i % 25))} INTO ${cn}`);
      const oldValue = getMetric("rocksdb_cache_auto_refill_loaded_total");
      db._query(`FOR doc IN ${cn} REPLACE doc WITH {_from: doc._from, _to: doc._to, value: doc.value + 1} INTO ${cn}`);
      const newValue = getMetric("rocksdb_cache_auto_refill_loaded_total");

      assertTrue(newValue - oldValue >= 2 * n, { oldValue, newValue });
      waitForMetrics();
      runCheck();
    },
    
  };
}

jsunity.run(AutoRefillIndexCachesEdge);
return jsunity.done();
