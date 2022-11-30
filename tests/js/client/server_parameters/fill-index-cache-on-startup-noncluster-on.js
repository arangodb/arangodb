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

const cn = 'UnitTestsCollection';
const db = require('@arangodb').db;

if (getOptions === true) {
  return {
    'rocksdb.auto-fill-index-caches-on-startup' : 'true',
    'server.statistics' : 'false',
    runSetup: true,
  };
}
if (runSetup === true) {
  let c = db._createEdgeCollection(cn);
  let docs = [];
  for (let i = 0; i < 10 * 1000; ++i) {
    docs.push({ _key: 'test' + i, _from: "v/test" + i, _to: "v/test" + (i % 25), value1: "test" + i, value2: i });
  }
  c.insert(docs);
  c.ensureIndex({ type: "persistent", fields: ["value1", "value2"], cacheEnabled: true });
  // index without a cache
  c.ensureIndex({ type: "persistent", fields: ["value2"] });
  return true;
}

const jsunity = require('jsunity');
const getMetric = require('@arangodb/test-helper').getMetricSingle;
const time = require('internal').time;

function FillIndexCacheOnStartup() {
  'use strict';

  return {
    setUpAll: function() {
      let end = time() + 60;
      while (time() < end) {
        const value = getMetric("rocksdb_cache_full_index_refills_total");
        if (value >= 3) {
          // the two sides of the edge index count as 2 indexes here...
          return;
        }
        require('internal').sleep(0.5);
      }
      assertFalse(true, "timeout during setup");
    },
    
    testCacheResultEdgeFrom: function() {
      const q = `FOR i IN 0..9999 FOR doc IN ${cn} FILTER doc._from == CONCAT('v/test', i) RETURN doc._from`;
      let crsr = db._query(q);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, 'v/test' + i);
      });
      // unfortunately there are no guarantees how much of the data
      // will have been cached, as loading indexes into the cache
      // is only best effort.
      let stats = crsr.getExtra().stats;
      assertTrue(stats.cacheHits > 0, stats);
    },
    
    testCacheResultEdgeTo: function() {
      const q = `FOR i IN 0..24 FOR doc IN ${cn} FILTER doc._to == CONCAT('v/test', i) RETURN doc._to`;
      let crsr = db._query(q);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, 'v/test' + Math.floor(i / 400), {val, i});
      });
      // unfortunately there are no guarantees how much of the data
      // will have been cached, as loading indexes into the cache
      // is only best effort.
      let stats = crsr.getExtra().stats;
      assertTrue(stats.cacheHits > 0, stats);
    },
    
    testCacheResultVPack: function() {
      const q = `FOR i IN 0..9999 FOR doc IN ${cn} FILTER doc.value1 == CONCAT('test', i) && doc.value2 == i RETURN doc._key`;
      let crsr = db._query(q);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, 'test' + i);
      });
      // unfortunately there are no guarantees how much of the data
      // will have been cached, as loading indexes into the cache
      // is only best effort.
      let stats = crsr.getExtra().stats;
      assertTrue(stats.cacheHits > 0, stats);
    },
    
    testCacheResultVPackUnindexedField: function() {
      const q = `FOR i IN 0..9999 FOR doc IN ${cn} FILTER doc.value2 == i RETURN doc._key`;
      let crsr = db._query(q);
      let res = crsr.toArray();
      assertEqual(10000, res.length);
      res.forEach((val, i) => {
        assertEqual(val, 'test' + i);
      });
      let stats = crsr.getExtra().stats;
      assertEqual(0, stats.cacheHits);
    },

  };
}

jsunity.run(FillIndexCacheOnStartup);
return jsunity.done();
