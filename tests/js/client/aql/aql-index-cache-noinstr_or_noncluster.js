/*jshint globalstrict:true, strict:true, esnext: true */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, fail */

"use strict";

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
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const jsunity = require("jsunity");
const internal = require('internal');
const { deriveTestSuite } = require('@arangodb/test-helper');
const isCluster = require("internal").isCluster();
let IM = global.instanceManager;
const canUseFailAt = IM.debugCanUseFailAt();  
// we do not want to run these tests for cluster sanitizer runs since they are very resource intensive

const cn = "UnitTestsCollection";

function CreateSuite () {
  let checkIndex = (options, cacheEnabled) => {
    if (cacheEnabled !== undefined) {
      options.cacheEnabled = cacheEnabled;
    }

    let c = db._create(cn);
    try {
      let idx = c.ensureIndex(options);
      assertEqual(options.type, idx.type);
      assertTrue(idx.hasOwnProperty("cacheEnabled"));
      assertEqual(Boolean(cacheEnabled), idx.cacheEnabled);

      c.indexes().filter((idx) => idx.type === options.type).forEach((idx) => {
        assertEqual(Boolean(cacheEnabled), idx.cacheEnabled);
      });
    } finally {
      db._drop(cn);
    }
  };

  return {
    testCreateNonUniquePersistentIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "persistent", fields: ["value1"] }, value);
      });
    },
    
    testCreateUniquePersistentIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "persistent", fields: ["value1"], unique: true }, value);
      });
    },
    
    testCacheEnabledFlagDoesNotMakeADifference1 : function () {
      let c = db._create(cn);
      try {
        assertEqual(1, c.indexes().length);

        let idx1 = c.ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
        assertTrue(idx1.isNewlyCreated);
        assertTrue(idx1.cacheEnabled);
        assertEqual(2, c.indexes().length);
        
        // create same index but with different cacheEnabled flag value
        let idx2 = c.ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: false });
        assertFalse(idx2.isNewlyCreated);
        assertTrue(idx2.cacheEnabled);
        assertEqual(2, c.indexes().length);
        
        assertEqual(idx1.id, idx2.id);
      } finally {
        db._drop(cn);
      }
    },

    testCacheEnabledFlagDoesNotMakeADifference2 : function () {
      let c = db._create(cn);
      try {
        assertEqual(1, c.indexes().length);

        let idx1 = c.ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: false });
        assertTrue(idx1.isNewlyCreated);
        assertFalse(idx1.cacheEnabled);
        assertEqual(2, c.indexes().length);
        
        // create same index but with different cacheEnabled flag value
        let idx2 = c.ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
        assertFalse(idx2.isNewlyCreated);
        assertFalse(idx2.cacheEnabled);
        assertEqual(2, c.indexes().length);
        
        assertEqual(idx1.id, idx2.id);
      } finally {
        db._drop(cn);
      }
    }
  };
}

function FiguresSuite () {
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value: i });
  }

  let checkIndex = (options, cacheEnabled) => {
    options.name = "testCache";
    if (cacheEnabled !== undefined) {
      options.cacheEnabled = cacheEnabled;
    }

    let c = db._create(cn);
    try {
      let idx = c.ensureIndex(options);
      assertEqual(options.type, idx.type);
      assertTrue(idx.hasOwnProperty("cacheEnabled"));
      assertEqual(Boolean(cacheEnabled), idx.cacheEnabled);

      if (!isCluster) {
        // cluster unfortunately does not report any index figures
        // (and never did)
        let figures = c.indexes(true).filter((idx) => idx.name === 'testCache')[0];
        if (!cacheEnabled) {
          assertFalse(figures.figures.cacheInUse, figures);
          assertEqual(0, figures.figures.cacheSize, figures);
          assertEqual(0, figures.figures.cacheUsage, figures);
          assertFalse(figures.figures.hasOwnProperty("cacheLifeTimeHitRate"), figures);
          assertFalse(figures.figures.hasOwnProperty("cacheWindowedTimeHitRate"), figures);
        } else {
          assertTrue(figures.figures.cacheInUse, figures);
          assertNotEqual(0, figures.figures.cacheSize, figures);
          assertEqual(0, figures.figures.cacheUsage, figures);
          assertTrue(figures.figures.cacheSize > figures.figures.cacheUsage, figures);
          assertTrue(figures.figures.hasOwnProperty("cacheLifeTimeHitRate"), figures);
          assertTrue(figures.figures.hasOwnProperty("cacheWindowedHitRate"), figures);
          assertEqual(0, figures.figures.cacheLifeTimeHitRate, figures);
          assertEqual(0, figures.figures.cacheWindowedHitRate, figures);
        }

        if (cacheEnabled) {
          c.insert(docs);
      
          // figures shouldn't have changed
          let figures = c.indexes(true).filter((idx) => idx.name === 'testCache')[0];
          assertTrue(figures.figures.cacheInUse, figures);
          assertNotEqual(0, figures.figures.cacheSize, figures);
          assertEqual(0, figures.figures.cacheUsage, figures);
          assertTrue(figures.figures.hasOwnProperty("cacheLifeTimeHitRate"), figures);
          assertTrue(figures.figures.hasOwnProperty("cacheWindowedHitRate"), figures);
          assertEqual(0, figures.figures.cacheLifeTimeHitRate, figures);
          assertEqual(0, figures.figures.cacheWindowedHitRate, figures);

          // figures should now change due to reading the data into the cache
          for (let i = 0; i < 5; ++i) {
            db._query(`FOR i IN 1..1000 FOR doc IN ${cn} FILTER doc.value == i RETURN 1`);
          }
          
          figures = c.indexes(true).filter((idx) => idx.name === 'testCache')[0];
          assertNotEqual(0, figures.figures.cacheSize, figures);
          assertNotEqual(0, figures.figures.cacheUsage, figures);
          assertTrue(figures.figures.cacheSize > figures.figures.cacheUsage, figures);
          assertTrue(figures.figures.hasOwnProperty("cacheLifeTimeHitRate"), figures);
          assertTrue(figures.figures.hasOwnProperty("cacheWindowedHitRate"), figures);
          assertNotEqual(0, figures.figures.cacheLifeTimeHitRate, figures);
        }
      }
      
    } finally {
      db._drop(cn);
    }
  };

  return {
    testFiguresNonUniquePersistentIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "persistent", fields: ["value"] }, value);
      });
    },
    
    testFiguresUniquePersistentIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "persistent", fields: ["value"], unique: true }, value);
      });
    },
  };
}

function VPackIndexCacheModifySuite (unique) {
  const n = 2000;

  const maxTries = 30;
  
  let setFailurePointForPointLookup = () => {
    if (canUseFailAt) {
      IM.debugSetFailAt("VPackIndexFailWithoutCache");
    }
  };

  let insertDocuments = () => {
    let c = db._collection(cn);
    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i, value: i });
      if (docs.length === 1000) {
        c.insert(docs);
        docs = [];
      }
    }
  };

  return {
    setUp: function () {
      db._drop(cn);
      let c = db._create(cn);

      insertDocuments();
      
      if (canUseFailAt) {
        IM.debugClearFailAt();
      }

      c.ensureIndex({ type: "persistent", fields: ["value"], name: "UnitTestsIndex", unique, cacheEnabled: true });
    },
    
    tearDown: function () {
      if (canUseFailAt) {
        IM.debugClearFailAt();
      }
      db._drop(cn);
    },

    testPointLookupAndTruncate: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < maxTries; ++tries) {
        if (tries !== 0) {
          db[cn].truncate();
          insertDocuments();
        }
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value == i RETURN doc`);
        let result = qres.toArray();
        assertEqual(1000, result.length, tries);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual("test" + i, doc._key);
          assertEqual(i, doc.value);
        }
        let stats = qres.getExtra().stats;
        assertEqual(0, stats.cacheHits, stats);
        assertEqual(1000, stats.cacheMisses, stats);
        
        // query again (may hit the cache)
        qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value == i RETURN doc`);
        result = qres.toArray();
        assertEqual(1000, result.length, tries);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual("test" + i, doc._key);
          assertEqual(i, doc.value);
        }
        stats = qres.getExtra().stats;
        assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
        // upon last round of tries, assume we have read at least
        // _something_ from the cache. we cannot assume that everything
        // was read from the cache because we don't have control over
        // the entire caching subsystem from the tests and the interaction
        // between different caches, cache migration events etc.
        assertTrue(tries < maxTries - 1 || stats.cacheHits > 0, stats);
        if (stats.CacheHits > 0) {
          break;
        }
      }
    },
    
    testPointLookupAndRemove: function () {
      setFailurePointForPointLookup();

      let keys = [];
      for (let i = 0; i < n; ++i) {
        keys.push("test" + i);
      }
      for (let tries = 0; tries < maxTries; ++tries) {
        if (tries !== 0) {
          db[cn].remove(keys);
          insertDocuments();
        }
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value == i RETURN doc`);
        let result = qres.toArray();
        assertEqual(1000, result.length, tries);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual("test" + i, doc._key);
          assertEqual(i, doc.value);
        }
        let stats = qres.getExtra().stats;
        assertEqual(0, stats.cacheHits, stats);
        assertEqual(1000, stats.cacheMisses, stats);
        
        // query again (may hit the cache)
        qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value == i RETURN doc`);
        result = qres.toArray();
        assertEqual(1000, result.length, tries);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual("test" + i, doc._key);
          assertEqual(i, doc.value);
        }
        stats = qres.getExtra().stats;
        assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
        // upon last round of tries, assume we have read at least
        // _something_ from the cache. we cannot assume that everything
        // was read from the cache because we don't have control over
        // the entire caching subsystem from the tests and the interaction
        // between different caches, cache migration events etc.
        assertTrue(tries < maxTries - 1 || stats.cacheHits > 0, stats);
        if (stats.CacheHits > 0) {
          break;
        }
      }
    },
    
    testPointLookupAndUpdate: function () {
      setFailurePointForPointLookup();

      let keys = [];
      for (let i = 0; i < n; ++i) {
        keys.push("test" + i);
      }
      for (let tries = 0; tries < maxTries; ++tries) {
        if (tries !== 0) {
          let updates = [];
          for (let i = 0; i < n; ++i) {
            updates.push({ updated: i + tries });
          }
          db[cn].update(keys, updates);
          insertDocuments();
        }
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value == i RETURN doc`);
        let result = qres.toArray();
        assertEqual(1000, result.length, tries);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual("test" + i, doc._key);
          assertEqual(i, doc.value);
          if (tries > 0) {
            assertEqual(i + tries, doc.updated);
          }
        }
        let stats = qres.getExtra().stats;
        assertEqual(0, stats.cacheHits, stats);
        assertEqual(1000, stats.cacheMisses, stats);
        
        // query again (may hit the cache)
        qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value == i RETURN doc`);
        result = qres.toArray();
        assertEqual(1000, result.length, tries);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual("test" + i, doc._key);
          assertEqual(i, doc.value);
          if (tries > 0) {
            assertEqual(i + tries, doc.updated);
          }
        }
        stats = qres.getExtra().stats;
        assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
        // upon last round of tries, assume we have read at least
        // _something_ from the cache. we cannot assume that everything
        // was read from the cache because we don't have control over
        // the entire caching subsystem from the tests and the interaction
        // between different caches, cache migration events etc.
        assertTrue(tries < maxTries - 1 || stats.cacheHits > 0, stats);
        if (stats.CacheHits > 0) {
          break;
        }
      }
    },

  };
}

function VPackIndexCacheReadOnlySuite (unique, cacheEnabled) {
  const n = 5000;
  const maxTries = 10;
  
  let setFailurePointIfCacheUsed = () => {
    if (canUseFailAt) {
      // fails whenever the cache is used
      IM.debugSetFailAt("VPackIndexFailWithCache");
    }
  };

  let setFailurePointForPointLookup = () => {
    if (canUseFailAt) {
      // if cacheEnabled == true,  fails if the cache is not used.
      // if cacheEnabled == false, fails if the cache is used.
      IM.debugSetFailAt("VPackIndexFail" + (cacheEnabled ? "Without" : "With") + "Cache");
    }
  };

  return {
    // we can't use setUpAll here because we need new cache instances for
    // the collection for each test case.
    setUp: function () {
      db._drop(cn);
      let c = db._create(cn);

      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "testmann" + String(i).padStart(5, "0") });
        if (docs.length === 1000) {
          c.insert(docs);
          docs = [];
        }
      }
      
      if (canUseFailAt) {
        IM.debugClearFailAt();
      }

      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"], name: "UnitTestsIndex", unique, cacheEnabled });
    },
    
    tearDown: function () {
      if (canUseFailAt) {
        IM.debugClearFailAt();
      }
      db._drop(cn);
    },

    testFailurePoints: function () {
      if (canUseFailAt) {
        // test opposite failure point
        if (cacheEnabled) {
          IM.debugSetFailAt("VPackIndexFailWithCache");
        } else {
          IM.debugSetFailAt("VPackIndexFailWithoutCache");
        }

        try {
          db._query(`FOR doc IN ${cn} FILTER doc.value1 == 123 && doc.value2 == 'testmann00123' RETURN doc`);
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }
      }
    },
    
    testFullScanNonCovering: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let result = db._query(`FOR doc IN ${cn} SORT doc.value1 RETURN doc`).toArray();
      assertEqual(n, result.length);
      for (let i = 0; i < n; ++i) {
        const doc = result[i];
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value1);
        assertEqual("testmann" + String(i).padStart(5, "0"), doc.value2);
      }
    },
    
    testFullScanCovering: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let qres = db._query(`FOR doc IN ${cn} SORT doc.value1 RETURN doc.value1`);
      let result = qres.toArray();
      assertEqual(n, result.length);
      for (let i = 0; i < n; ++i) {
        assertEqual(i, result[i]);
      }
      let stats = qres.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
      assertEqual(0, stats.cacheMisses, stats);
    },
    
    testRangeScanNonCovering: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let qres = db._query(`FOR doc IN ${cn} FILTER doc.value1 >= 10 && doc.value1 < 1000 SORT doc.value1 RETURN doc`);
      let result = qres.toArray();
      assertEqual(990, result.length);
      for (let i = 0; i < result.length; ++i) {
        const doc = result[i];
        assertEqual("test" + (i + 10), doc._key);
        assertEqual(i + 10, doc.value1);
        assertEqual("testmann" + String(i + 10).padStart(5, "0"), doc.value2);
      }
      let stats = qres.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
      assertEqual(0, stats.cacheMisses, stats);
    },
    
    testRangeScanCovering: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let qres = db._query(`FOR doc IN ${cn} FILTER doc.value1 >= 10 && doc.value1 < 1000 SORT doc.value1 RETURN doc.value1`);
      let result = qres.toArray();
      assertEqual(990, result.length);
      for (let i = 0; i < result.length; ++i) {
        assertEqual(i + 10, result[i]);
      }
      let stats = qres.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
      assertEqual(0, stats.cacheMisses, stats);
    },
    
    testPointLookupOnlyMissesPartialCoverage: function () {
      setFailurePointIfCacheUsed();

      for (let tries = 0; tries < (cacheEnabled ? maxTries : 1); ++tries) {
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == 50000 + i RETURN doc`);
        let result = qres.toArray();
        assertEqual(0, result.length);
        let stats = qres.getExtra().stats;
        assertEqual(0, stats.cacheHits, stats);
        assertEqual(0, stats.cacheMisses, stats);
      }
    },
    
    testPointLookupOnlyMissesFullCoverage: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < (cacheEnabled ? maxTries : 1); ++tries) {
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i && doc.value2 == CONCAT('testmann', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(50000 + i))), i) RETURN doc`);
        let result = qres.toArray();
        assertEqual(0, result.length);
        let stats = qres.getExtra().stats;
        if (cacheEnabled) {
          if (tries === 0) {
            assertEqual(0, stats.cacheHits, stats);
            assertEqual(1000, stats.cacheMisses, stats);
          } else {
            assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
            if (stats.cacheHits > 0) {
              break;
            }
          }
        } else {
          assertEqual(0, stats.cacheHits, stats);
          assertEqual(0, stats.cacheMisses, stats);
        }
      }
    },
    
    testPointLookupNonCovering: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < (cacheEnabled ? maxTries : 1); ++tries) {
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i && doc.value2 == CONCAT('testmann', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i))), i) RETURN doc`);
        let result = qres.toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual("test" + i, doc._key);
          assertEqual(i, doc.value1);
          assertEqual("testmann" + String(i).padStart(5, "0"), doc.value2);
        }
        let stats = qres.getExtra().stats;
        if (cacheEnabled) {
          if (tries === 0) {
            assertEqual(0, stats.cacheHits, stats);
            assertEqual(1000, stats.cacheMisses, stats);
          } else {
            assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
            if (stats.cacheHits > 0) {
              break;
            }
          }
        } else {
          assertEqual(0, stats.cacheHits, stats);
          assertEqual(0, stats.cacheMisses, stats);
        }
      }
    },

    testPointLookupCovering1: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < (cacheEnabled ? maxTries : 1); ++tries) {
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i && doc.value2 == CONCAT('testmann', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i))), i) RETURN doc.value1`);
        let result = qres.toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          assertEqual(i, result[i]);
        }
        let stats = qres.getExtra().stats;
        if (cacheEnabled) {
          if (tries === 0) {
            assertEqual(0, stats.cacheHits, stats);
            assertEqual(1000, stats.cacheMisses, stats);
          } else {
            assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
            if (stats.cacheHits > 0) {
              break;
            }
          }
        } else {
          assertEqual(0, stats.cacheHits, stats);
          assertEqual(0, stats.cacheMisses, stats);
        }
      }
    },

    testPointLookupCovering2: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < (cacheEnabled ? maxTries : 1); ++tries) {
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i && doc.value2 == CONCAT('testmann', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i))), i) RETURN [doc.value1, doc.value2]`);
        let result = qres.toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual(i, doc[0]);
          assertEqual("testmann" + String(i).padStart(5, "0"), doc[1]);
        }
        let stats = qres.getExtra().stats;
        if (cacheEnabled) {
          if (tries === 0) {
            assertEqual(0, stats.cacheHits, stats);
            assertEqual(1000, stats.cacheMisses, stats);
          } else {
            assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
            if (stats.cacheHits > 0) {
              break;
            }
          }
        } else {
          assertEqual(0, stats.cacheHits, stats);
          assertEqual(0, stats.cacheMisses, stats);
        }
      }
    },
    
    testPointLookupCoveringCacheExplicitlyOn: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < (cacheEnabled ? maxTries : 1); ++tries) {
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} OPTIONS { useCache: true } FILTER doc.value1 == i && doc.value2 == CONCAT('testmann', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i))), i) RETURN [doc.value1, doc.value2]`);
        let result = qres.toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual(i, doc[0]);
          assertEqual("testmann" + String(i).padStart(5, "0"), doc[1]);
        }
        let stats = qres.getExtra().stats;
        if (cacheEnabled) {
          if (tries === 0) {
            assertEqual(0, stats.cacheHits, stats);
            assertEqual(1000, stats.cacheMisses, stats);
          } else {
            assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
            if (stats.cacheHits > 0) {
              break;
            }
          }
        } else {
          assertEqual(0, stats.cacheHits, stats);
          assertEqual(0, stats.cacheMisses, stats);
        }
      }
    },
    
    testPointLookupCoveringCacheDisabled: function () {
      setFailurePointIfCacheUsed();

      for (let tries = 0; tries < (cacheEnabled ? maxTries : 1); ++tries) {
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} OPTIONS { useCache: false }FILTER doc.value1 == i && doc.value2 == CONCAT('testmann', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i))), i) RETURN doc.value1`);
        let result = qres.toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          assertEqual(i, result[i]);
        }
        let stats = qres.getExtra().stats;
        assertEqual(0, stats.cacheHits, stats);
        assertEqual(0, stats.cacheMisses, stats);
      }
    },
    
    testMixedLookupNonCovering: function () {
      setFailurePointIfCacheUsed();

      let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i && doc.value2 >= 'test' RETURN doc`);
      let result = qres.toArray();
      assertEqual(1000, result.length);
      for (let i = 0; i < result.length; ++i) {
        const doc = result[i];
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value1);
        assertEqual("testmann" + String(i).padStart(5, "0"), doc.value2);
      }
      let stats = qres.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
      assertEqual(0, stats.cacheMisses, stats);
    },

  };
}

function VPackIndexCacheReadOnlyStoredValuesSuite (unique) {
  const n = 10000;
  const maxTries = 10;
  
  let setFailurePointIfCacheUsed = () => {
    if (canUseFailAt) {
      // fails whenever the cache is used
      IM.debugSetFailAt("VPackIndexFailWithCache");
    }
  };

  let setFailurePointForPointLookup = () => {
    if (canUseFailAt) {
      IM.debugSetFailAt("VPackIndexFailWithoutCache");
    }
  };

  return {
    // we can't use setUpAll here because we need new cache instances for
    // the collection for each test case.
    setUp: function () {
      db._drop(cn);
      let c = db._create(cn);

      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "testmann" + String(i).padStart(5, "0"), value3: i, value4: "test" + i });
        if (docs.length === 1000) {
          c.insert(docs);
          docs = [];
        }
      }
      
      if (canUseFailAt) {
        IM.debugClearFailAt();
      }

      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"], storedValues: ["value3", "value4"], name: "UnitTestsIndex", unique, cacheEnabled: true });
    },
    
    tearDown: function () {
      if (canUseFailAt) {
        IM.debugClearFailAt();
      }
      db._drop(cn);
    },

    testFullScanCovering: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let qres = db._query(`FOR doc IN ${cn} SORT doc.value1 RETURN doc.value3`);
      let result = qres.toArray();
      assertEqual(n, result.length);
      for (let i = 0; i < n; ++i) {
        assertEqual(i, result[i]);
      }
      let stats = qres.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
      assertEqual(0, stats.cacheMisses, stats);
    },
    
    testFullScanCoveringMultiple: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let qres = db._query(`FOR doc IN ${cn} SORT doc.value1 RETURN [doc.value1, doc.value2, doc.value3, doc.value4]`);
      let result = qres.toArray();
      assertEqual(n, result.length);
      for (let i = 0; i < n; ++i) {
        assertEqual(i, result[i][0]);
        assertEqual("testmann" + String(i).padStart(5, "0"), result[i][1]);
        assertEqual(i, result[i][2]);
        assertEqual("test" + i, result[i][3]);
      }
      let stats = qres.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
      assertEqual(0, stats.cacheMisses, stats);
    },
    
    testRangeScanCovering: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let qres = db._query(`FOR doc IN ${cn} FILTER doc.value1 >= 10 && doc.value1 < 1000 SORT doc.value1 RETURN doc.value3`);
      let result = qres.toArray();
      assertEqual(990, result.length);
      for (let i = 0; i < result.length; ++i) {
        assertEqual(i + 10, result[i]);
      }
      let stats = qres.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
      assertEqual(0, stats.cacheMisses, stats);
    },
    
    testRangeScanCoveringMultiple: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let qres = db._query(`FOR doc IN ${cn} FILTER doc.value1 >= 10 && doc.value1 < 1000 SORT doc.value1 RETURN [doc.value1, doc.value2, doc.value3, doc.value4]`);
      let result = qres.toArray();
      assertEqual(990, result.length);
      for (let i = 0; i < result.length; ++i) {
        assertEqual(i + 10, result[i][0]);
        assertEqual("testmann" + String(i + 10).padStart(5, "0"), result[i][1]);
        assertEqual(i + 10, result[i][2]);
        assertEqual("test" + (i + 10), result[i][3]);
      }
      let stats = qres.getExtra().stats;
      assertEqual(0, stats.cacheHits, stats);
      assertEqual(0, stats.cacheMisses, stats);
    },
    
    testPointLookupCovering1: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < maxTries; ++tries) {
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i && doc.value2 == CONCAT('testmann', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i))), i) RETURN doc.value3`);
        let result = qres.toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          assertEqual(i, result[i]);
        }
        let stats = qres.getExtra().stats;
        if (tries === 0) {
          assertEqual(0, stats.cacheHits, stats);
          assertEqual(1000, stats.cacheMisses, stats);
        } else {
          assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
          assertTrue(stats.cacheHits > 0 || tries < maxTries + 1);
          if (stats.cacheHits > 0) {
            break;
          }
        }
      }
    },

    testPointLookupCovering2: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < maxTries; ++tries) {
        let qres = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i && doc.value2 == CONCAT('testmann', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i))), i) RETURN [doc.value1, doc.value2, doc.value3, doc.value4]`);
        let result = qres.toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual(i, doc[0]);
          assertEqual("testmann" + String(i).padStart(5, "0"), doc[1]);
          assertEqual(i, doc[2]);
          assertEqual("test" + i, doc[3]);
        }
        let stats = qres.getExtra().stats;
        if (tries === 0) {
          assertEqual(0, stats.cacheHits, stats);
          assertEqual(1000, stats.cacheMisses, stats);
        } else {
          assertEqual(1000, stats.cacheHits + stats.cacheMisses, stats);
          assertTrue(stats.cacheHits > 0 || tries < maxTries + 1);
          if (stats.cacheHits > 0) {
            break;
          }
        }
      }
    },

  };
}

function PersistentIndexNonUniqueHugeValueSuite () {
  const maxTries = 10;
  const n = 200000; 
  
  let setFailurePointForPointLookup = () => {
    if (canUseFailAt) {
      IM.debugSetFailAt("VPackIndexFailWithoutCache");
    }
  };

  return {
    setUpAll : function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], cacheEnabled: true });
      let docs = [];
      // create a "super node", which will not be stored in the cache
      for (let i = 0; i < n; ++i) {
        docs.push({ value: "testmann42" });
        if (docs.length === 10000) {
          c.insert(docs);
          docs = [];
        }
      }
      // create a non-"super node"
      c.insert({ value: "foobar" });
    },

    tearDownAll : function () {
      db._drop(cn);
    },

    testHugeValue : function () {
      setFailurePointForPointLookup();

      let c = db._collection(cn);

      for (let tries = 0; tries < maxTries; ++tries) {
        let qres = db._query(`FOR doc IN ${cn} FILTER doc.value == 'testmann42' RETURN 1`);
        let result = qres.toArray();
        assertEqual(n, result.length);
        let stats = qres.getExtra().stats;
        assertEqual(0, stats.cacheHits, stats);
        // the value for the super node can never be served from the cache
        assertEqual(1, stats.cacheMisses, stats);
      }
    },
    
    testSmallValue : function () {
      setFailurePointForPointLookup();

      let c = db._collection(cn);

      for (let tries = 0; tries < maxTries; ++tries) {
        let qres = db._query(`FOR doc IN ${cn} FILTER doc.value == 'foobar' RETURN 1`);
        let result = qres.toArray();
        assertEqual(1, result.length);
        let stats = qres.getExtra().stats;
        if (tries === 0) {
          assertEqual(0, stats.cacheHits, stats);
          assertEqual(1, stats.cacheMisses, stats);
        } else {
          assertEqual(1, stats.cacheHits + stats.cacheMisses, stats);
          assertTrue(stats.cacheHits === 1 || tries < maxTries + 1);
          if (stats.cacheHits > 0) {
            break;
          }
        }
      }
    },

  };
}

function OtherIndexesSuite () {
  let checkIndex = (options, cacheEnabled) => {
    if (cacheEnabled !== undefined) {
      options.cacheEnabled = cacheEnabled;
    }

    let c = db._create(cn);
    try {
      let idx = c.ensureIndex(options);
      assertEqual(options.type, idx.type);
      assertFalse(idx.hasOwnProperty("cacheEnabled"));

      c.indexes().forEach((idx) => {
        assertFalse(idx.hasOwnProperty("cacheEnabled"));
      });
    } finally {
      db._drop(cn);
    }
  };

  return {
    testEdgeIndex : function () {
      let c = db._createEdgeCollection(cn);

      try {
        assertEqual(1, c.indexes().filter((idx) => idx.type === 'edge').length);
        // edge indexes do not expose the "cacheEnabled" attribute so far.
        c.indexes().forEach((idx) => {
          assertFalse(idx.hasOwnProperty("cacheEnabled"));
        });
      } finally {
        db._drop(cn);
      }
    },
    
    testInvertedIndex : function () {
      // inverted index should not show the "cacheEnabled" property
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "inverted", fields: [{ name: "value1" }] }, value);
      });
    },

    testTtlIndex : function () {
      // ttl index should not show the "cacheEnabled" property
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "ttl", fields: ["value1"], expireAfter: 600 }, value);
      });
    },
    
    testGeoIndex : function () {
      // geo index should not show the "cacheEnabled" property
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "geo", fields: ["value1", "value2"] }, value);
      });
    },
    
    testFulltextIndex : function () {
      // fulltext index should not show the "cacheEnabled" property
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "fulltext", fields: ["value1"], minLength: 3 }, value);
      });
    },
  };
}

function PersistentIndexNonUniqueModifySuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheModifySuite(/*unique*/ false), suite, '_nonUnique');
  return suite;
}

function PersistentIndexUniqueModifySuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheModifySuite(/*unique*/ true), suite, '_unique');
  return suite;
}

function PersistentIndexNonUniqueReadOnlyCacheDisabledSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheReadOnlySuite(/*unique*/ false, /*cacheEnabled*/ false), suite, '_nonUnique_cacheDisabled');
  return suite;
}

function PersistentIndexNonUniqueReadOnlyCacheEnabledSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheReadOnlySuite(/*unique*/ false, /*cacheEnabled*/ true), suite, '_nonUnique_cacheEnabled');
  return suite;
}

function PersistentIndexNonUniqueReadOnlyStoredValuesSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheReadOnlyStoredValuesSuite(/*unique*/ false), suite, '_nonUnique_storedValues');
  return suite;
}

function PersistentIndexUniqueReadOnlyCacheDisabledSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheReadOnlySuite(/*unique*/ true, /*cacheEnabled*/ false), suite, '_unique_cacheDisabled');
  return suite;
}

function PersistentIndexUniqueReadOnlyCacheEnabledSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheReadOnlySuite(/*unique*/ true, /*cacheEnabled*/ true), suite, '_unique_cacheEnabled');
  return suite;
}

function PersistentIndexUniqueReadOnlyStoredValuesSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheReadOnlyStoredValuesSuite(/*unique*/ true), suite, '_unique_storedValues');
  return suite;
}

jsunity.run(CreateSuite);
jsunity.run(FiguresSuite);
jsunity.run(PersistentIndexNonUniqueModifySuite);
jsunity.run(PersistentIndexUniqueModifySuite);
jsunity.run(PersistentIndexNonUniqueHugeValueSuite);
jsunity.run(PersistentIndexNonUniqueReadOnlyCacheDisabledSuite);
jsunity.run(PersistentIndexNonUniqueReadOnlyCacheEnabledSuite);
jsunity.run(PersistentIndexNonUniqueReadOnlyStoredValuesSuite);
jsunity.run(PersistentIndexUniqueReadOnlyCacheDisabledSuite);
jsunity.run(PersistentIndexUniqueReadOnlyCacheEnabledSuite);
jsunity.run(PersistentIndexUniqueReadOnlyStoredValuesSuite);
jsunity.run(OtherIndexesSuite);

return jsunity.done();
