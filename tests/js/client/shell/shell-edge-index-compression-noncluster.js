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
const runWithRetry = require('@arangodb/test-helper').runWithRetry;
const sleep = require('internal').sleep;

const cn = 'UnitTestsCollection';

function EdgeIndexCompressionSuite() {
  'use strict';

  let retryCb = () => {
    sleep(0.25);
  };
     
  return {
    tearDown: function() {
      db._drop(cn);
    },
   
    testNonCompressibleValuesFrom: function() {
      const n = 5000;

      let c = db._createEdgeCollection(cn);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        // only one edge per _from value. this is below the compressibility threshold.
        docs.push({ _from: "v/test" + i, _to: "v/test" + i });
      }
      c.insert(docs);
      
      const oldUncompressedSize = getMetric("arangodb_edge_cache_uncompressed_entries_size");
      const oldCompressedSize = getMetric("arangodb_edge_cache_effective_entries_size");

      runWithRetry(() => {
        let result = db._query(`FOR i IN 0..${n - 1} FOR e IN ${cn} FILTER e._from == CONCAT('v/test', i) RETURN e`);
        assertEqual(n, result.toArray().length);
        let stats = result.getExtra().stats;
        assertEqual(n, stats.cacheHits + stats.cacheMisses, stats);
        assertTrue(stats.cacheHits > 0, stats);
        
        const newUncompressedSize = getMetric("arangodb_edge_cache_uncompressed_entries_size");
        const newCompressedSize = getMetric("arangodb_edge_cache_effective_entries_size");
        // total values should have increased for both uncompressed and effective payload sizes
        assertTrue(newUncompressedSize > oldUncompressedSize, { newUncompressedSize, oldUncompressedSize });
        assertTrue(newCompressedSize > oldCompressedSize, { newCompressedSize, oldCompressedSize });
        // increase in uncompressed size should be equal to the increase in effective size, because data was not compressible
        assertEqual(newCompressedSize - oldCompressedSize, newUncompressedSize - oldUncompressedSize, { newCompressedSize, oldCompressedSize, newUncompressedSize, oldUncompressedSize });
      }, retryCb);
    },
    
    testNonCompressibleValuesTo: function() {
      const n = 5000;

      let c = db._createEdgeCollection(cn);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        // only one edge per _to value. this is below the compressibility threshold.
        docs.push({ _from: "v/test" + i, _to: "v/test" + i });
      }
      c.insert(docs);
      
      const oldUncompressedSize = getMetric("arangodb_edge_cache_uncompressed_entries_size");
      const oldCompressedSize = getMetric("arangodb_edge_cache_effective_entries_size");

      runWithRetry(() => {
        let result = db._query(`FOR i IN 0..${n - 1} FOR e IN ${cn} FILTER e._to == CONCAT('v/test', i) RETURN e`);
        assertEqual(n, result.toArray().length);
        let stats = result.getExtra().stats;
        assertEqual(n, stats.cacheHits + stats.cacheMisses, stats);
        assertTrue(stats.cacheHits > 0, stats);
        
        const newUncompressedSize = getMetric("arangodb_edge_cache_uncompressed_entries_size");
        const newCompressedSize = getMetric("arangodb_edge_cache_effective_entries_size");
        // total values should have increased for both uncompressed and effective payload sizes
        assertTrue(newUncompressedSize > oldUncompressedSize, { newUncompressedSize, oldUncompressedSize });
        assertTrue(newCompressedSize > oldCompressedSize, { newCompressedSize, oldCompressedSize });
        // increase in uncompressed size should be equal to the increase in effective size, because data was not compressible
        assertEqual(newCompressedSize - oldCompressedSize, newUncompressedSize - oldUncompressedSize, { newCompressedSize, oldCompressedSize, newUncompressedSize, oldUncompressedSize });
      }, retryCb);
    },
    
    testCompressibleValuesFrom: function() {
      const n = 5000;

      let c = db._createEdgeCollection(cn);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        // 50 edges per _from value. this is above the compressibility threshold.
        docs.push({ _from: "v/test" + (i % 100), _to: "v/test" + i });
      }
      c.insert(docs);
      
      const oldUncompressedSize = getMetric("arangodb_edge_cache_uncompressed_entries_size");
      const oldCompressedSize = getMetric("arangodb_edge_cache_effective_entries_size");

      runWithRetry(() => {
        let result = db._query(`FOR i IN 0..99 FOR e IN ${cn} FILTER e._from == CONCAT('v/test', i) RETURN e`);
        assertEqual(n, result.toArray().length);
        let stats = result.getExtra().stats;
        assertEqual(100, stats.cacheHits + stats.cacheMisses, stats);
        assertTrue(stats.cacheHits > 0, stats);
        
        const newUncompressedSize = getMetric("arangodb_edge_cache_uncompressed_entries_size");
        const newCompressedSize = getMetric("arangodb_edge_cache_effective_entries_size");
        // total values should have increased for both uncompressed and effective payload sizes
        assertTrue(newUncompressedSize > oldUncompressedSize, { newUncompressedSize, oldUncompressedSize });
        assertTrue(newCompressedSize > oldCompressedSize, { newCompressedSize, oldCompressedSize });
        // increase in uncompressed size should be greater than the increase in effective size, because data was compressible
        assertTrue(newCompressedSize - oldCompressedSize < newUncompressedSize - oldUncompressedSize, { newCompressedSize, oldCompressedSize, newUncompressedSize, oldUncompressedSize });
      }, retryCb);
    },
    
    testCompressibleValuesTo: function() {
      const n = 5000;

      let c = db._createEdgeCollection(cn);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        // 50 edges per _to value. this is above the compressibility threshold.
        docs.push({ _from: "v/test" + i, _to: "v/test" + (i % 100) });
      }
      c.insert(docs);
      
      const oldUncompressedSize = getMetric("arangodb_edge_cache_uncompressed_entries_size");
      const oldCompressedSize = getMetric("arangodb_edge_cache_effective_entries_size");

      runWithRetry(() => {
        let result = db._query(`FOR i IN 0..99 FOR e IN ${cn} FILTER e._to == CONCAT('v/test', i) RETURN e`);
        assertEqual(n, result.toArray().length);
        let stats = result.getExtra().stats;
        assertEqual(100, stats.cacheHits + stats.cacheMisses, stats);
        assertTrue(stats.cacheHits > 0, stats);
        
        const newUncompressedSize = getMetric("arangodb_edge_cache_uncompressed_entries_size");
        const newCompressedSize = getMetric("arangodb_edge_cache_effective_entries_size");
        // total values should have increased for both uncompressed and effective payload sizes
        assertTrue(newUncompressedSize > oldUncompressedSize, { newUncompressedSize, oldUncompressedSize });
        assertTrue(newCompressedSize > oldCompressedSize, { newCompressedSize, oldCompressedSize });
        // increase in uncompressed size should be greater than the increase in effective size, because data was compressible
        assertTrue(newCompressedSize - oldCompressedSize < newUncompressedSize - oldUncompressedSize, { newCompressedSize, oldCompressedSize, newUncompressedSize, oldUncompressedSize });
      }, retryCb);
    },

  };
}

jsunity.run(EdgeIndexCompressionSuite);
return jsunity.done();
