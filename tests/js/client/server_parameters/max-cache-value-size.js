/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'cache.max-cache-value-size': '1MB',
  };
}

const jsunity = require('jsunity');
const internal = require('internal');
const db = require('@arangodb').db;

function CacheValueSizeSuite() {
  'use strict';

  const cn = "UnitTestsCollection";
  
  return {
    tearDown: function () {
      db._drop(cn);
    },

    testDocumentsEligibleForCaching: function () {
      // turn on document caching
      const c = db._create(cn);
      c.properties({ cacheEnabled: true });
      assertTrue(c.properties().cacheEnabled);

      let payload = Array(4).join("foobar");
      const n = 15;
      for (let i = 0; i < n; ++i) {
        c.insert({ _key: "test" + i, payload, size: payload.length });
        payload += payload;
        assertTrue(payload.length < 1024 * 1024, payload.length);
      }
      assertTrue(payload.length > 512 * 1024, payload.length);

      let stats = {};
      let tries = 0;
      while (++tries < 100) {
        // loop until all documents most documents are served from the in-memory document cache
        let res = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc._key == CONCAT('test', i) RETURN doc`);
        assertEqual(n, res.toArray().length);
        stats = res.getExtra().stats;
        // we don't require every single document to be returned from the cache,
        // simply because the cache does not guarantee to cache every single
        // document. there may be collisions inside cache buckets and cache
        // buckets running full that can prevent every single document from being cached.
        if (stats.cacheHits >= n - 2) {
          break;
        }
        internal.sleep(0.25);
      }
      assertTrue(stats.cacheHits >= n -2, stats);
    },
    
    testDocumentsNotEligibleForCaching: function () {
      // turn on document caching
      const c = db._create(cn);
      c.properties({ cacheEnabled: true });
      assertTrue(c.properties().cacheEnabled);
      c.ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: false, unique: true });

      let payload = Array(1024 * 1024).join("fux");
      assertTrue(payload.length > 1024 * 1024, payload.length);
      const n = 10;
      for (let i = 0; i < n; ++i) {
        c.insert({ value: "test" + i, payload, size: payload.length });
      }

      // try 3 times to avoid things from getting into the cache in the next rounds
      for (let i = 0; i < 3; ++i) {
        let res = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc.value == CONCAT('test', i) RETURN doc`);
        assertEqual(n, res.toArray().length);
        let stats = res.getExtra().stats;
        // we never expect to see a cache hit here
        assertEqual(0, stats.cacheHits, stats);
        internal.sleep(0.25);
      }
    },
    
    testPersistentIndexEligibleForCaching: function () {
      const c = db._create(cn);
      assertFalse(c.properties().cacheEnabled);
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: ["payload"], cacheEnabled: true });

      let payload = Array(4).join("foobar");
      const n = 15;
      for (let i = 0; i < n; ++i) {
        c.insert({ value: "test" + i, payload, size: payload.length });
        payload += payload;
        assertTrue(payload.length < 1024 * 1024, payload.length);
      }
      assertTrue(payload.length > 512 * 1024, payload.length);

      let stats = {};
      let tries = 0;
      while (++tries < 100) {
        // loop until all documents most documents are served from the in-memory document cache
        let res = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc.value == CONCAT('test', i) RETURN doc.payload`);
        assertEqual(n, res.toArray().length);
        stats = res.getExtra().stats;
        // we don't require every single document to be returned from the cache,
        // simply because the cache does not guarantee to cache every single
        // document. there may be collisions inside cache buckets and cache
        // buckets running full that can prevent every single document from being cached.
        if (stats.cacheHits >= n - 2) {
          break;
        }
        internal.sleep(0.25);
      }
      assertTrue(stats.cacheHits >= n -2, stats);
    },
    
    testPersistentIndexNotEligibleForCaching: function () {
      const c = db._create(cn);
      assertFalse(c.properties().cacheEnabled);
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: ["payload"], cacheEnabled: true });

      let payload = Array(1024 * 1024).join("fux");
      assertTrue(payload.length > 1024 * 1024, payload.length);
      const n = 10;
      for (let i = 0; i < n; ++i) {
        c.insert({ value: "test" + i, payload, size: payload.length });
      }

      // try 3 times to avoid things from getting into the cache in the next rounds
      for (let i = 0; i < 3; ++i) {
        let res = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc.value == CONCAT('test', i) RETURN doc.payload`);
        assertEqual(n, res.toArray().length);
        let stats = res.getExtra().stats;
        // we never expect to see a cache hit here
        assertEqual(0, stats.cacheHits, stats);
        internal.sleep(0.25);
      }
    },
    
    testUniquePersistentIndexEligibleForCaching: function () {
      const c = db._create(cn);
      assertFalse(c.properties().cacheEnabled);
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: ["payload"], unique: true, cacheEnabled: true });

      let payload = Array(4).join("foobar");
      const n = 15;
      for (let i = 0; i < n; ++i) {
        c.insert({ value: "test" + i, payload, size: payload.length });
        payload += payload;
        assertTrue(payload.length < 1024 * 1024, payload.length);
      }
      assertTrue(payload.length > 512 * 1024, payload.length);

      let stats = {};
      let tries = 0;
      while (++tries < 100) {
        // loop until all documents most documents are served from the in-memory document cache
        let res = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc.value == CONCAT('test', i) RETURN doc.payload`);
        assertEqual(n, res.toArray().length);
        stats = res.getExtra().stats;
        // we don't require every single document to be returned from the cache,
        // simply because the cache does not guarantee to cache every single
        // document. there may be collisions inside cache buckets and cache
        // buckets running full that can prevent every single document from being cached.
        if (stats.cacheHits >= n - 2) {
          break;
        }
        internal.sleep(0.25);
      }
      assertTrue(stats.cacheHits >= n -2, stats);
    },
    
    testUniquePersistentIndexNotEligibleForCaching: function () {
      const c = db._create(cn);
      assertFalse(c.properties().cacheEnabled);
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: ["payload"], unique: true, cacheEnabled: true });

      let payload = Array(1024 * 1024).join("fux");
      assertTrue(payload.length > 1024 * 1024, payload.length);
      const n = 10;
      for (let i = 0; i < n; ++i) {
        c.insert({ value: "test" + i, payload, size: payload.length });
      }

      // try 3 times to avoid things from getting into the cache in the next rounds
      for (let i = 0; i < 3; ++i) {
        let res = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc.value == CONCAT('test', i) RETURN doc.payload`);
        assertEqual(n, res.toArray().length);
        let stats = res.getExtra().stats;
        // we never expect to see a cache hit here
        assertEqual(0, stats.cacheHits, stats);
        internal.sleep(0.25);
      }
    },
    
    testEdgeIndexEligibleForCaching: function () {
      const c = db._createEdgeCollection(cn);
      assertFalse(c.properties().cacheEnabled);

      let payload = Array(4).join("foobar");
      let docs = [];
      const n = 10;
      for (let i = 0; i < n; ++i) {
        for (let j = 0; j < 1000; ++j) {
          docs.push({ _from: "test/" + i, _to: "test/" + j });
        }
        c.insert(docs);
        docs = [];
      }

      let stats = {};
      let tries = 0;
      while (++tries < 100) {
        // loop until all documents most documents are served from the in-memory document cache
        let res = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc._from == CONCAT('test/', i) RETURN doc._to`);
        assertEqual(n * 1000, res.toArray().length);
        stats = res.getExtra().stats;
        // we don't require every single document to be returned from the cache,
        // simply because the cache does not guarantee to cache every single
        // document. there may be collisions inside cache buckets and cache
        // buckets running full that can prevent every single document from being cached.
        if (stats.cacheHits >= n - 2) {
          break;
        }
        internal.sleep(0.25);
      }
      assertTrue(stats.cacheHits >= n -2, stats);
    },
    
    testEdgeIndexNotEligibleForCaching: function () {
      const c = db._createEdgeCollection(cn);
      assertFalse(c.properties().cacheEnabled);

      let payload = Array(4).join("foobar");
      let docs = [];
      const n = 6;
      for (let i = 0; i < n; ++i) {
        for (let j = 0; j < 40000; ++j) {
          docs.push({ _from: "test/" + i, _to: "test/thisissomeintentionallylongvertexname" + j });
          if (docs.length === 5000) {
            c.insert(docs);
            docs = [];
          }
        }
      }
      
      // try 3 times to avoid things from getting into the cache in the next rounds
      for (let i = 0; i < 3; ++i) {
        let res = db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc._from == CONCAT('test/', i) RETURN doc._to`);
        assertEqual(n * 40000, res.toArray().length);
        let stats = res.getExtra().stats;
        // we never expect to see a cache hit here
        assertEqual(0, stats.cacheHits, stats);
        internal.sleep(0.25);
      }
    },

  };
}

jsunity.run(CacheValueSizeSuite);
return jsunity.done();
