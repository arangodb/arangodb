/*jshint globalstrict:true, strict:true, esnext: true */
/*global assertEqual, assertTrue, assertFalse, fail */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const jsunity = require("jsunity");
const { deriveTestSuite } = require('@arangodb/test-helper');

function CreateSuite () {
  const cn = "UnitTestsCollection";
        
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
    testNonUniquePersistentIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "persistent", fields: ["value1"] }, value);
      });
    },
    
    testUniquePersistentIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "persistent", fields: ["value1"], unique: true }, value);
      });
    },
    
    testNonUniqueHashIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "hash", fields: ["value1"] }, value);
      });
    },
    
    testUniqueHashIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "hash", fields: ["value1"], unique: true }, value);
      });
    },

    testNonUniqueSkiplistIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "skiplist", fields: ["value1"] }, value);
      });
    },
    
    testUniqueSkiplistIndex : function () {
      [ false, true, undefined ].forEach((value) => {
        checkIndex({ type: "skiplist", fields: ["value1"], unique: true }, value);
      });
    },
  };
}

function VPackIndexCacheSuite (type, unique, cacheEnabled) {
  const internal = require('internal');
  const canUseFailAt =  internal.debugCanUseFailAt();

  const cn = "UnitTestsCollection";
  const n = 10000;
  
  let setFailurePointIfCacheUsed = () => {
    if (canUseFailAt) {
      // fails whenever the cache is used
      internal.debugSetFailAt("VPackIndexFailWithCache");
    }
  };

  let setFailurePointForPointLookup = () => {
    if (canUseFailAt) {
      // if cacheEnabled == true,  fails if the cache is not used.
      // if cacheEnabled == false, fails if the cache is used.
      internal.debugSetFailAt("VPackIndexFail" + (cacheEnabled ? "Without" : "With") + "Cache");
    }
  };

  return {
    setUpAll: function () {
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

      c.ensureIndex({ type, fields: ["value1", "value2"], name: "UnitTestsIndex", unique, cacheEnabled });
    },
    
    tearDownAll: function () {
      db._drop(cn);
    },

    setUp: function () {
      if (canUseFailAt) {
        internal.debugClearFailAt();
      }
    },

    tearDown: function () {
      if (canUseFailAt) {
        internal.debugClearFailAt();
      }
    },
    
    testFailurePoints: function () {
      if (canUseFailAt) {
        // test opposite failure point
        if (cacheEnabled) {
          internal.debugSetFailAt("VPackIndexFailWithCache");
        } else {
          internal.debugSetFailAt("VPackIndexFailWithoutCache");
        }

        try {
          db._query(`FOR doc IN ${cn} FILTER doc.value1 == 123 RETURN doc`);
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

      let result = db._query(`FOR doc IN ${cn} SORT doc.value1 RETURN doc.value1`).toArray();
      assertEqual(n, result.length);
      for (let i = 0; i < n; ++i) {
        assertEqual(i, result[i]);
      }
    },
    
    testRangeScanNonCovering: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let result = db._query(`FOR doc IN ${cn} FILTER doc.value1 >= 10 && doc.value1 < 1000 SORT doc.value1 RETURN doc`).toArray();
      assertEqual(990, result.length);
      for (let i = 0; i < result.length; ++i) {
        const doc = result[i];
        assertEqual("test" + (i + 10), doc._key);
        assertEqual(i + 10, doc.value1);
        assertEqual("testmann" + String(i + 10).padStart(5, "0"), doc.value2);
      }
    },
    
    testRangeScanCovering: function () {
      // full scan must never used the cache
      setFailurePointIfCacheUsed();

      let result = db._query(`FOR doc IN ${cn} FILTER doc.value1 >= 10 && doc.value1 < 1000 SORT doc.value1 RETURN doc.value1`).toArray();
      assertEqual(990, result.length);
      for (let i = 0; i < result.length; ++i) {
        assertEqual(i + 10, result[i]);
      }
    },
    
    testPointLookupNonCovering1: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < (cacheEnabled ? 3 : 1); ++tries) {
        let result = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i RETURN doc`).toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual("test" + i, doc._key);
          assertEqual(i, doc.value1);
          assertEqual("testmann" + String(i).padStart(5, "0"), doc.value2);
        }
      }
    },

    testPointLookupNonCovering2: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < (cacheEnabled ? 3 : 1); ++tries) {
        let result = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i && doc.value2 == CONCAT('testmann', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i))), i) RETURN doc`).toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual("test" + i, doc._key);
          assertEqual(i, doc.value1);
          assertEqual("testmann" + String(i).padStart(5, "0"), doc.value2);
        }
      }
    },
    
    testPointLookupCovering1: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < (cacheEnabled ? 3 : 1); ++tries) {
        let result = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i RETURN doc.value1`).toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          assertEqual(i, result[i]);
        }
      }
    },

    testPointLookupCovering2: function () {
      setFailurePointForPointLookup();

      for (let tries = 0; tries < (cacheEnabled ? 3 : 1); ++tries) {
        let result = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i RETURN [doc.value1, doc.value2]`).toArray();
        assertEqual(1000, result.length);
        for (let i = 0; i < result.length; ++i) {
          const doc = result[i];
          assertEqual(i, doc[0]);
          assertEqual("testmann" + String(i).padStart(5, "0"), doc[1]);
        }
      }
    },
    
    testMixedLookupNonCovering: function () {
      setFailurePointIfCacheUsed();

      let result = db._query(`FOR i IN 0..999 FOR doc IN ${cn} FILTER doc.value1 == i && doc.value2 >= 'test' RETURN doc`).toArray();
      assertEqual(1000, result.length);
      for (let i = 0; i < result.length; ++i) {
        const doc = result[i];
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value1);
        assertEqual("testmann" + String(i).padStart(5, "0"), doc.value2);
      }
    },

  };
}

function OtherIndexesSuite () {
  const cn = "UnitTestsCollection";
        
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


function PersistentIndexNonUniqueCacheDisabledSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheSuite("persistent", /*unique*/ false, /*cacheEnabled*/ false), suite, '_persistent_cacheDisabled');
  return suite;
}

function PersistentIndexNonUniqueCacheEnabledSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheSuite("persistent", /*unique*/ false, /*cacheEnabled*/ true), suite, '_persistent_cacheEnabled');
  return suite;
}

function PersistentIndexUniqueCacheDisabledSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheSuite("persistent", /*unique*/ true, /*cacheEnabled*/ false), suite, '_persistent_unique_cacheDisabled');
  return suite;
}

function PersistentIndexUniqueCacheEnabledSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexCacheSuite("persistent", /*unique*/ true, /*cacheEnabled*/ true), suite, '_persistent_unique_cacheEnabled');
  return suite;
}

jsunity.run(CreateSuite);
jsunity.run(PersistentIndexNonUniqueCacheDisabledSuite);
jsunity.run(PersistentIndexNonUniqueCacheEnabledSuite);
jsunity.run(PersistentIndexUniqueCacheDisabledSuite);
jsunity.run(PersistentIndexUniqueCacheEnabledSuite);
jsunity.run(OtherIndexesSuite);

return jsunity.done();
